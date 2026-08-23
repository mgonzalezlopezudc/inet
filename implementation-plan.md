# TGn wideband MIMO channel implementation plan

> The initial channel/MRC plan is retained below as delivered design history. The
> follow-on plan for antenna selection, combining, STBC, and spatial-stream
> detection starts at [Follow-on MIMO receiver strategies implementation
> plan](#follow-on-mimo-receiver-strategies-implementation-plan).

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

# Follow-on MIMO receiver strategies implementation plan

## Outcome and bounded scope

Extend the delivered TGn matrix path into a receiver-local, covariance-aware
MIMO processing pipeline. The pipeline must retain the desired and interfering
Rx-by-Tx channel matrices until receiver processing and provide these selectable
strategies:

- receive-antenna subset selection;
- selection combining for one non-STBC spatial stream;
- maximum-SINR, equivalently single-stream linear-MMSE, combining;
- HT STBC decoding, initially the 1-spatial-stream to 2-space-time-stream
  Alamouti layout;
- spatial-stream detection with ZF, linear MMSE, and perfect-cancellation
  MMSE-SIC.

The first implementation targets single-user HT-mixed-format HT20/HT40 with
1--4 spatial streams, direct spatial mapping, equal stream-power allocation,
and the existing TGn limit of 1--8 physical antennas per side. HT-greenfield is
a later adapter. Legacy DSSS, HR-DSSS, and OFDM preamble/header/control traffic
must retain the current one-stream behavior. Omitting the wideband channel model
must retain the original dimensional-medium path exactly.

Terms are fixed as follows:

- **Receive-antenna selection** means a local, perfect-CSI selection of a subset
  of receive rows before baseband processing. It is an INET receiver policy, not
  IEEE antenna selection (ASEL). Normative ASEL sounding, RF-chain mapping,
  selection staleness, and capability exchange are outside this plan.
  `numberOfActiveReceiveChains` resolves once at initialization (`-1` means all
  physical receive antennas) to a value in `[1,N_RX]`; fixed selection supplies
  exactly that many unique row indices, while optimal selection enumerates every
  subset of that size.
- **Selection combining** chooses one branch from the already active receive
  rows. It does not coherently sum branch phases. With one active RF chain it is
  mathematically the same choice as one-antenna subset selection; the two remain
  separate only because subset selection models the available hardware and the
  combiner models baseband processing.
- **Maximum-SINR/MMSE combining** is the one-stream linear combiner whose weight
  direction is `Rz^-1 a`. It reduces to the existing MRC result when noise is
  spatially white and there is no overlapping interference.
- **SIC** means a packet-level, perfect-cancellation upper-bound model with an
  MMSE front end and deterministic strongest-post-detection-SINR ordering. It
  does not model symbol decisions or error propagation and must be named and
  documented accordingly.

The following remain outside this first receiver release:

- normative IEEE ASEL procedures;
- transmit beamforming, expansion mapping, MU-MIMO/OFDMA, and joint multiuser
  detection;
- VHT, HE, and EHT STBC adapters;
- imperfect channel estimation, synchronization error, RF-chain coupling,
  correlated receiver thermal noise, and time-varying selection state learned
  from sounding;
- sample-level symbol reconstruction or error-propagating SIC;
- claims that the existing scalar NIST error model validates MIMO/STBC PER.

The receiver algorithms are complete when their analytical outputs, integration,
configuration gates, and legacy behavior pass the focused gates below. MIMO PER
calibration is a distinct follow-up unless an authoritative curve/dataset is
added to this scope.

## Standards authority and explicit model policies

Use IEEE Std 802.11-2024 as the normative source. The processed IEEE Std
802.11-2024 document was fresh when this plan was written; no PDF inspection was
required. Other documents in the local corpus are outside this receiver plan and
need not share that processing status.

- Clause 19.3.12.1, corpus chunk `80211ax-2024:chunk:08164`, defines
  `y[k] = H[k] x[k] + n[k]`, with receive antennas as matrix rows and transmit
  antennas as columns.
- Clause 19.3.3, chunk `08058`, places STBC after constellation mapping and
  before spatial mapping in the transmit chain.
- Clause 19.3.11.9.2 and Table 19-18, chunks `08148`--`08150`, define HT STBC
  mapping, including the signs/conjugations and `N_STS > N_SS` relationship.
- Table 19-11 and Figure 19-6, chunks `08094`--`08097`, define the complete
  48-bit HT-SIG representation and LSB-first field order. Table 19-12, chunk
  `08105`, derives `N_STS` from MCS-derived `N_SS` and the two-bit STBC field;
  `N_SS` is not a separately transmitted HT-SIG field.
- Clause 19.3.9.4.4 and Figure 19-8, chunks `08099` and `08101`, define the
  protected HT-SIG bits, CRC polynomial/complement, and `c7`-first transmission.
  Clause 19.3.21, chunk `08235`, distinguishes invalid-CRC `FormatViolation`
  from reserved or unsupported HT-SIG combinations reported as
  `UnsupportedRate`.
- Clause 19.3.5, chunk `08064`, makes NSS part of authoritative MCS metadata:
  MCS 0--7 and the special MCS 32 are one stream, while MCS 8--31 are
  multi-stream EQM. This is why NSS must not be inferred by counting the current
  mode object's non-null modulation pointers.
- Table 19-10, chunk `08092`, and Clause 19.3.11.11.2, chunk `08157`, define HT
  cyclic shifts and frequency-dependent spatial mapping. Clause 19.3.9.4.6 and
  Tables 19-13--19-14, chunks `08104`--`08107`, define the `N_STS`/`N_ESS`
  dependent HT-LTF count and its 4 us mixed-format duration.
- Clause 10.16, chunk `05229`, requires STBC use to be gated by the local Tx-STBC
  capability and every intended receiver's advertised Rx-STBC capability.
  Clause 9.4.2.54.2 and Table 9-224, chunks `02636` and `02639`, define the one-bit
  HT Tx-STBC field and 0--3 HT Rx-STBC encoding. Clause 9.4.2.54.4, chunk `02643`,
  defines the Supported MCS Set, and Clause 10.6.5.8, chunk `05157`, also gates
  transmission by the peer's MCS and bandwidth support. This plan adds
  receive-only support first and does not enable ordinary STBC rate selection
  until management owns the required per-peer capability state.
- Tables 9-62 and 9-64--9-69, chunks `01753`, `01762`, `01768`, `01774`,
  `01780`, `01786`, and `01792`, place HT Capabilities in Beacon, Association,
  Reassociation, Probe Request, and Probe Response exchanges. Those typed on-air
  fields, not a PHY mode pointer or duplicate NED parameter, are the
  peer-capability authority.
- Clause 3.1 and Clause 10.34.1--10.34.2, chunks `00350` and
  `05493`--`05497`, define IEEE ASEL. The stateless, perfect-CSI subset selector
  in this plan must not be labeled as that feature.

The standard does not prescribe selection combining, maximum-SINR, MMSE, ZF, or
SIC as receiver implementations. The ZF text in Clause 19.3.18.7.4, chunk
`08208`, is transmitter EVM test instrumentation, not a normative receiver
architecture. All corresponding algorithms, update cadence, numerical policy,
and fallback behavior below are therefore explicit INET modeling policies.

The nonnormative TGn contribution remains the authority for channel generation,
not for receiver algorithms. Do not change its profiles, stochastic processes,
normalization, link state, or frequency/time response to make a receiver test
pass.

## Current gap and invariants to preserve

The current path is:

```text
Ieee80211Transmitter
  -> scalar transmit-power function + IIeee80211Mode
  -> TgnChannelModel creates full H(t,f)
  -> DimensionalMediumAnalogModel selects one Tx column
  -> ChannelMatrixCombiner immediately computes sum_rx |H[rx,column]|^2
  -> scalar ChannelMatrixSnir
  -> scalar Ieee80211 error decision
```

Specific gaps that must be removed deliberately are:

- `TgnChannelModel::validateMode()` rejects `NSS != 1` and nonzero HT STBC.
- `DimensionalMediumAnalogModel` owns a medium-global
  `selectedTransmitAntenna`, rather than consuming a transmitter-owned spatial
  plan.
- `ChannelMatrixReceptionAnalogModel` retains one selected column and scalar
  decoded/CCA powers, but no stream mapping or stream covariance.
- `ChannelMatrixNoise` retains interfering receptions but not the resulting
  receive-branch covariance.
- overlapping matrix interference throws for more than one receive antenna.
- `ChannelMatrixSnir` exposes only one scalar function.
- HT STBC is hardcoded to zero, the HT PHY header carries no STBC field, and its
  serializer does not encode one.

Preserve these invariants throughout the refactor:

1. `H(t,f)` remains immutable, Rx-row/Tx-column, absolute-time based, finite,
   query-order independent, and RNG-free during evaluation.
2. Deterministic path loss and snapshot shadowing are applied exactly once.
3. Total transmit PSD is conserved: NSS-dimensional source fractions are
   nonnegative and sum to one, every resolved STS slot covariance has unit
   trace, and adding streams or STBC never multiplies radiated power.
4. CCA observes physical energy independently of whether a decoder supports the
   PPDU. A decoder failure never makes the medium appear idle.
5. Receiver strategy selection is fixed after initialization. No cached
   reception, noise, or SNIR may depend on a mutable strategy parameter.
6. The legacy/header path and the HT-data path may use different spatial plans;
   STBC or spatial multiplexing must not be applied to legacy control portions.
7. Common wireless code contains no IEEE mode, frame, or amendment knowledge.

## Target data flow and ownership

The target flow is:

```text
Ieee80211Transmitter
  -> immutable, segment-specific SpatialTransmissionPlan
       {NSS, NSTS, Q(t,f), resolved STS covariance, STBC descriptor}
  -> TgnChannelModel produces full H(t,f) without receiver-algorithm gating
  -> ChannelMatrixReceptionAnalogModel retains H, large-scale PSD, and plan
  -> ChannelMatrixNoise retains background PSD and interfering matrix receptions
  -> receiver-local ChannelMatrixReceptionProcessor
       forms desired effective channel and interference covariance
       applies receive-antenna subset policy
       dispatches one of:
         single stream -> selection or maximum-SINR combiner
         HT STBC       -> code-aware augmented decoder
         multiple NSS  -> ZF, MMSE, or perfect-cancellation MMSE-SIC
  -> ChannelMatrixSnir retains per-part/per-stream functions
  -> explicit scalar compatibility mapper feeds existing threshold/error APIs
```

Ownership rules:

- The transmitter owns the immutable spatial transmission plan.
- `TgnChannelModel` owns only channel-realization state and validates only facts
  required to create a channel snapshot.
- The receiver owns strategy configuration and calls its composed policy
  submodules.
- The medium owns physical reception/noise construction and caching, but does
  not choose an IEEE receiver algorithm.
- Each reception-processing call returns an immutable result. Strategy modules
  retain no per-packet mutable state.

`DimensionalMediumAnalogModel::computeSNIR()` may discover the optional generic
`IChannelMatrixReceiver` capability through
`reception->getReceiverRadio()->getReceiver()`. It must not downcast to
`Ieee80211Receiver`. Same-instant processing uses direct C++ calls, not zero-time
messages.

## Common matrix and spatial contracts

Add generic artifacts under
`src/inet/physicallayer/wireless/common/`:

| Artifact | Responsibility |
|---|---|
| `analogmodel/common/ChannelMatrixAlgebra.{h,cc}` | Conjugate transpose, multiplication, Hermitian products, rank/condition checks, and factorization-based solves for matrices up to 8x8. |
| `analogmodel/common/SpatialTransmissionPlan.{h,cc}` | Immutable ordered segments with NSS/NSTS, Tx-by-STS mapping, symbol-power fractions, resolved STS covariance, and optional space-time-code descriptor. |
| `analogmodel/common/SpaceTimeCodeDescriptor.{h,cc}` | Typed code layout, slot encoder matrices/conjugation flags, normalization, and joint augmented covariance without IEEE field interpretation. |
| `contract/packetlevel/ISpatialTransmission.h` | Optional technology-neutral transmission capability exposing the immutable spatial plan without widening scalar `ITransmission` semantics. |
| `analogmodel/dimensional/receiver/ChannelMatrixReceptionContext.{h,cc}` | Immutable desired/interferer matrices, large-scale PSDs, background covariance, PPDU part, and exact `(time,frequency)` evaluation point. |
| `analogmodel/dimensional/receiver/ChannelMatrixDetectionResult.{h,cc}` | Selected Rx rows, weights or detection order, per-stream desired/residual PSD, and per-stream SINR. |
| `contract/packetlevel/IChannelMatrixReceiver.{h,ned}` | Optional receiver capability exposing its receiver-local processor. |
| `contract/packetlevel/IChannelMatrixReceptionProcessor.{h,ned}` | Composes subset selection, one-stream combining, STBC decoding, spatial detection, and scalar compatibility mapping. |
| `contract/packetlevel/IReceiveAntennaSelectionPolicy.{h,ned}` | Produces deterministic candidate receive-row subsets. |
| `contract/packetlevel/ISingleStreamCombiner.{h,ned}` | Processes one non-STBC spatial stream. |
| `contract/packetlevel/ISpaceTimeBlockDecoder.{h,ned}` | Converts a typed STBC descriptor into a two-symbol augmented detection problem. |
| `contract/packetlevel/ISpatialStreamDetector.{h,ned}` | Processes multiple simultaneous non-STBC streams. |
| `contract/packetlevel/ISpatialSnir.h` | Adds per-part/per-stream SNIR access without weakening the existing `ISnir` API. |
| `contract/packetlevel/ISpatialStreamSnirMapper.{h,ned}` | Maps explicit per-part/per-stream SINR to the scalar compatibility API. |

Concrete policy modules belong in the generic `analogmodel/dimensional/receiver`
package unless they interpret IEEE fields:

- `AllReceiveAntennasSelectionPolicy`;
- `FixedReceiveAntennaSelectionPolicy`;
- `OptimalReceiveAntennaSelectionPolicy`;
- `MaximumRatioCombiner` as the compatibility default;
- `SelectionCombiner`;
- `MaximumSinrCombiner`;
- `ZeroForcingSpatialStreamDetector`;
- `MinimumMeanSquareErrorSpatialStreamDetector`;
- `PerfectCancellationSuccessiveInterferenceCancellationSpatialStreamDetector`;
- `MinimumSpatialStreamSnirMapper`.

The processor is a compound NED module with interface-typed submodules. Avoid a
single enum-driven class containing all algorithms. The optimal subset policy
enumerates candidates; the processor scores each candidate with the configured
downstream combiner/decoder/detector and chooses the largest minimum per-stream
SINR. The current eight-antenna bound makes exhaustive enumeration tractable.
Ties use lexicographically ordered receive indices.

Expose the capability and composition declaratively on each IEEE radio/receiver:

| NED surface | Initial choices/default |
|---|---|
| `htTxStbc` on `Ieee80211Radio` | Boolean local Table 9-224 capability, default false; it does not by itself authorize transmission to a peer. |
| `htRxStbc` on `Ieee80211Radio` | Integer Table 9-224 encoding `0..3`, default 0; the radio constructs the sole immutable HT capability value. |
| `channelMatrixReceptionProcessor.numberOfActiveReceiveChains` | `-1` means all physical Rx chains; otherwise fixed `1..N_RX`. |
| `channelMatrixReceptionProcessor.receiveAntennaSelection.typename` | `AllReceiveAntennasSelectionPolicy` by default; fixed and optimal policies selectable. |
| `channelMatrixReceptionProcessor.singleStreamCombiner.typename` | `MaximumRatioCombiner` by default; `SelectionCombiner` or `MaximumSinrCombiner` selectable. |
| `channelMatrixReceptionProcessor.spaceTimeBlockDecoder.typename` | Empty by default; `Ieee80211HtAlamoutiDecoder` only under the IEEE receiver. |
| `channelMatrixReceptionProcessor.spatialStreamDetector.typename` | Empty by default; ZF, linear-MMSE, or explicitly named perfect-cancellation MMSE-SIC selectable. |
| `channelMatrixReceptionProcessor.spatialStreamSnirMapper.typename` | `MinimumSpatialStreamSnirMapper` when a spatial receiver is enabled. |
| `channelMatrixReceptionProcessor.maximumMaterializedResourceCells` | Checked positive safety limit; freeze the default from HT20/HT40 maximum-duration benchmarks and fail rather than coarsen when exceeded. |

The processor owns the active-chain cardinality. The all-antenna policy is valid
only when it equals `N_RX`; the fixed policy must return its exact configured
unique indices; the optimal policy must enumerate all and only subsets of that
cardinality. An empty decoder/detector yields a typed unsupported-layout result,
not an implicit fallback.

Extend, rather than replace, these existing values:

- `ChannelMatrixReceptionAnalogModel` retains the snapshot, deterministic
  large-scale power, and spatial plan. Its inherited power is the total desired
  receive energy used by existing observation code, not a claim that all streams
  were decoded.
- `ChannelMatrixNoise` retains the scalar CCA aggregate for existing listening
  logic plus immutable copied interferer descriptors sufficient to form decoder
  covariance; it does not borrow receptions from other cache entries.
- `ChannelMatrixSnir` implements `ISpatialSnir`, retains the immutable detection
  result functions, and defines the ordinary `ISnir` summary as the conservative
  minimum across required parts and streams.

`SignalPowerInd` continues to report aggregate desired receive energy. Add a
separate typed `SpatialReceptionInd` only if a consumer needs selected rows,
per-stream power, or detection order; do not overload the existing scalar tag
with vector semantics.

## Spatial transmission and IEEE adaptation

An immutable spatial plan is a prerequisite, not an optional refinement. It
contains ordered, nonoverlapping time segments so HT training boundaries are not
lost inside the coarse `SignalPart` values. Each segment contains:

```text
numberOfSpatialStreams       NSS
numberOfSpaceTimeStreams     NSTS
transmitMapping              Q, dimensions N_TX x N_STS
symbolPowerFractions         p[s], length NSS, sum(p) = 1
spaceTimeStreamCovariance    Csts[l], N_STS x N_STS for code slot l
spaceTimeCode                none or typed code/layout descriptor
```

Every `Csts[l]` is finite, Hermitian positive semidefinite, and has unit trace.
For non-STBC transmission, `NSS=NSTS` and `Csts=diag(p)`. For STBC, the code
descriptor maps the NSS input symbols to each STS slot, including conjugation
and amplitude normalization, and derives both each slot covariance and the
joint two-slot augmented covariance. This separates NSS-dimensional source
power from NSTS-dimensional radiated power and prevents either dimensional
mismatch or STBC power duplication.

For the first release:

- legacy preamble/header/control parts use one stream and the first configured
  transmit antenna;
- HT-SIG in mixed format retains the existing robust one-stream representation;
  HT-STF, every required HT-LTF, and HT Data have distinct segment boundaries;
- all HT fields from HT-STF onward, including STBC Data, use the same direct
  mapping and CSD on a given subcarrier. For non-STBC data, source streams map
  one-to-one to STS and use equal fractions `1/NSS`. The plan resolves
  `Qk = D * diag(exp(-j*2*pi*k*deltaF*tau[s]))`, where `D` maps each STS to its
  configured transmit antenna and `tau` comes from Table 19-10 (`0`;
  `0,-400 ns`; `0,-400,-200 ns`; or `0,-400,-200,-600 ns` for 1--4 STS);
- HT 1SS-to-2STS STBC data uses the exact Table 19-18 Alamouti signs and
  conjugations with amplitude `alpha=1/sqrt(2)`, so each slot has
  `Csts=diag(1/2,1/2)` and the PPDU's total PSD is unchanged. Its two encoded
  STS pass through the same `Qk`/CSD as HT-STF and HT-LTF;
- the number of HT-DLTF segments is 1, 2, 4, or 4 for 1, 2, 3, or 4 STS;
  HT-ELTF segments follow `N_ESS`, and every mixed-format HT-LTF is 4 us. The
  first release uses perfect CSI rather than estimating it from those symbols,
  but still models their duration, energy, mapping, and CSD;
- invalid dimensions, duplicate/out-of-range transmit indices, nonfinite mapping
  coefficients, invalid power/covariance normalization, or inconsistent segment
  boundaries are configuration/programming errors.

Modify the IEEE-specific surface:

| Artifact | Planned change |
|---|---|
| `packetlevel/Ieee80211Transmission.{h,cc,msg}` | Implement the generic spatial-transmission capability and retain an immutable segment-specific plan, including HT-STF/HT-LTF boundaries inside coarse signal parts. Edit the `.msg` source only; never generated `_m.*`. |
| `packetlevel/Ieee80211Transmitter.{h,cc,ned}` | Derive the plan once from the authoritative mode and transmitter configuration. Validate `NSS <= NSTS <= N_TX`. |
| `mode/Ieee80211HtMode.{h,cc}` and `mode/Ieee80211ModeSet.{h,cc}` | Represent STBC as mode data instead of hardcoded zero and include it in mode lookup/cache identity. Add a receive-support/canonicalization path separate from the rate-selectable mode list; do not make an explicit receive-only STBC mode eligible for transmission. |
| `mode/Ieee80211HtCapabilities.{h,cc}` | Immutable radio-owned HT capability value containing Table 9-224 `txStbc`/`rxStbc`, Supported MCS Set, and supported widths. It is the sole local source for PHY gates and management advertisement. |
| `packetlevel/Ieee80211PhyModeResolver.{h,cc}` | First canonicalizes the complete PHY header plus preamble/transmit-channel context into a receiver-independent immutable PPDU description and derived NSS/NSTS; a separate receiver-local validator applies local MCS/width/capability/decoder support. Neither path falls back to sender-selected mode metadata. |
| `mode/Ieee80211HtSpaceTimeCodeBuilder.{h,cc}` | Sole IEEE owner of Table 19-18 layout selection, signs/conjugations, `alpha`, and covariance construction; returns the generic immutable descriptor consumed by both transmit planning and receive decoding. |
| `packetlevel/Ieee80211PhyHeader.msg` and `Ieee80211PhyHeaderSerializer.{h,cc}` | Represent and serialize all six HT-SIG octets: MCS, CBW, HT Length, Smoothing, Not Sounding, reserved, Aggregation, STBC, FEC Coding, Short GI, NESS, CRC, and tail. Follow Figure 19-6 bit positions/LSB-first order; derive NSS from MCS and NSTS from NSS+STBC instead of inventing an NSS field. |
| `packetlevel/Ieee80211Radio.{h,cc,ned}` | Own `Ieee80211HtCapabilities` from `htTxStbc=false`, `htRxStbc=0`, supported-MCS, and channel-width inputs; populate the complete typed HT header and expose that one local capability value to receiver, transmitter, and management advertisement. It does not place receiver-local support status in a transmission. |
| `packetlevel/Ieee80211Receiver.{h,cc,ned}` | Implement `IChannelMatrixReceiver`, own the processor submodule, gate supported PPDU parts/layouts against the radio-owned capability, and expose the explicit scalar-SNIR compatibility policy. Replace pointer-membership reception gating with receive-support validation of the canonical mode while preserving a separate rate-selectable-mode gate. |
| `packetlevel/receiver/Ieee80211HtAlamoutiDecoder.{h,cc,ned}` | Consume the shared Table 19-18 descriptor and a slot-specific augmented context (`H0/H1` plus full covariance); retain the current equal-slot overload only as a checked convenience wrapper. Do not duplicate table interpretation, signs, or normalization. |
| `packetlevel/errormodel/Ieee80211ErrorModelBase.{h,cc}` and `Ieee80211BerTableErrorModel.{h,cc}` | Consume the header-derived canonical mode for header/data lengths, bitrate, and error calculation; never read the optional sender-selected mode as receive authority. |
| `channelmodel/TgnChannelModel.cc` | Remove receiver-strategy/NSS/STBC rejection. Retain dimensional representation, antenna-count, carrier, and snapshot validation. |
| `packetlevel/Ieee80211TgnRadioMedium.ned` | Stop owning `selectedTransmitAntenna`; receiver/transmitter policy is configured on the corresponding radio submodules. |

Removing `selectedTransmitAntenna` from the medium is a configuration migration.
The replacement is a transmitter-local ordered antenna-index list whose default
starts at antenna 0, so an unmodified one-stream configuration retains the same
column and MRC result. Do not keep two authoritative parameters.

Receive-only STBC support may be configured and tested without enabling normal
STBC transmission selection. The transmission-level
`computeIsReceptionPossible(listening,transmission)` remains a physical
technology/band delivery filter so unsupported PPDUs still affect CCA. The
reception-decision path applies header-derived receiver-local support instead of
requiring pointer membership in the transmitter's selectable mode set. Local
STBC support requires `radio.htRxStbc >= NSS` and a configured decoder;
`htRxStbc=0` preserves the current default. Enabling STBC in rate selection
remains blocked until the transmitter can query the peer's advertised Rx-STBC
capability as required by Clause 10.16.

The transmission distinguishes optional `transmitterMode` metadata from a
required receiver-independent `canonicalPhyDescription` resolved once from the
completed on-air header, preamble format, and transmit-channel context. Spatial
plan construction consumes that description. Each receiver separately validates
its tuning, local MCS/width support, Rx-STBC capability, active processor, and
decoder against the same description; that receiver-local status is never
stored on a shared/broadcast transmission. Error calculation,
`Ieee80211ModeInd`, and decapsulation consume the canonical mode only after the
local validator succeeds. Construction succeeds without `transmitterMode`; if
present metadata contradicts the header-derived description, construction fails
before medium caching.

## Covariance and receiver mathematics

At each exact `(time,frequency)` point and non-STBC code slot, form:

```text
Gd = Hd * Qd
Ad = sqrt(Pd) * Gd * diag(sqrt(pd))

Rz = N0 * I
   + sum_j Pj * Hj * Qj * Csts,j(t,f) * Qj^H * Hj^H
```

`P` is the already path-loss/obstacle-loss-adjusted scalar PSD. Snapshot
shadowing is already inside `H`. The existing scalar background-noise PSD is an
independent equal noise PSD on each receiver chain, so its covariance is
`N0 I`; correlated receiver noise requires a future contract. Desired non-STBC
streams are initially independent, hence `Ad` uses `diag(sqrt(pd))`. Each
interferer contributes its resolved NSTS-dimensional slot covariance, so STBC
and non-STBC interferers use the same radiated-power contract. Validate that
every covariance is finite, Hermitian, positive semidefinite, and correctly
dimensioned; positive background noise makes the solve positive definite.

For an STBC desired signal or interferer, form the exact two-slot augmented
model. If `u0,u1` are the transmitted NSTS vectors, the code descriptor exposes
`Cbar = E([u0;conj(u1)] [u0;conj(u1)]^H)`, including any cross-slot terms. Apply
the correspondingly augmented channel/mapping operator and use
`Rbar_z = diag(N0 I,N0 I) + sum_j Pj Hbar_j Qbar_j Cbar_j Qbar_j^H Hbar_j^H`.
Resolve each interferer's own segment and code-slot indices at the two absolute
desired-symbol times; do not assume that overlapping STBC pairs are aligned.
Do not approximate an STBC interferer by two independent scalar streams unless
its derived `Cbar` proves that equivalence. Per-slot unit-trace and augmented
two-slot trace-two checks are mandatory power-conservation gates.

CCA remains separate:

```text
ccaSignalPower(t,f) = P * trace(G * Csts(t,f) * G^H)
```

The current scalar background-noise contribution is counted once by the CCA
policy for backward compatibility. Decoder covariance and CCA therefore have
deliberately distinct APIs.

For a selected receive-row set `S`, reduce both the desired matrix and
covariance before decoding:

```text
AS = selectRows(Ad, S)
RzS = selectRowsAndColumns(Rz, S)
```

Strategy semantics are:

| Strategy | Required computation | Defined failure/result |
|---|---|---|
| Selection combining | For one column `a`, select `argmax_i |a_i|^2 / Rz[i,i]`. Output only that branch. | Reject NSS != 1 or STBC. Zero channel yields zero SINR. |
| Maximum-SINR/MMSE combining | Solve `Rz w = a`; normalize `w` to unit Euclidean norm; output `|w^H a|^2 / (w^H Rz w)`. | Reject nonpositive/invalid covariance; never form an explicit inverse. |
| ZF | Solve the full-column-rank least-squares problem and compute each row's desired, residual cross-stream, and projected covariance power. | `Nr < NSS` or rank/condition failure yields a typed undecodable result, not NaN and not implicit MMSE fallback. |
| Linear MMSE | Compute a factorization-based equivalent of `W = A^H (A A^H + Rz)^-1`; include residual other-stream power in each output SINR. | Remains finite under rank deficiency when `Rz` is positive definite; zero desired channel yields zero SINR. |
| Perfect-cancellation MMSE-SIC | Recompute the MMSE front end over remaining columns, select the largest predicted post-detection SINR, remove that column perfectly, and repeat. Return results in original stream order plus detection order. | Ties use lowest original stream index. It never claims to model decision errors. |

Use conjugate transpose everywhere required. Do not use the ordinary transpose
implemented for reciprocal channel snapshots in receiver algebra.

Weight scaling is frozen, not left to each strategy. Selection weights are unit
basis vectors. Every one-stream weight, every spatial-detector row, and every
augmented STBC output weight is normalized to unit Euclidean norm before
reporting desired/residual output PSD or applying receiver sensitivity. SINR is
scale invariant, but this convention gives the signal-only output powers one
physical meaning across strategies. Algebra tests may additionally inspect an
unnormalized canonical ZF solution to verify `W A = I`; the immutable detection
result contains normalized rows.

ZF is covariance-whitened: solve the factorization-based equivalent of
`W = (A^H Rz^-1 A)^-1 A^H Rz^-1`. This still enforces `W A = I` for a
full-column-rank channel; with `Rz = N0 I` it reduces to ordinary ZF. Compute
`W` through solves/factorizations, not either displayed inverse.

Factorization policy:

- use a Hermitian positive-definite solve for covariance/MMSE systems;
- use a rank-revealing factorization or a rank-checked Gram solve for ZF;
- never construct a matrix inverse as an intermediate value;
- freeze a relative rank/condition threshold in code and test just above and
  below it; do not rely on platform epsilon;
- return a defined zero/undecodable value for physical channel degeneracy and
  throw only for invalid configuration, malformed metadata, or nonfinite math.

With spatially white noise and one stream, the maximum-SINR direction is `a`
and the result is the existing selected-column MRC gain divided by `N0`. This
equality is the backward-compatibility oracle.

## STBC processing order

Do not treat the two STS of an STBC transmission as independent spatial streams
and do not pre-combine them as a single selected column.

`Ieee80211HtSpaceTimeCodeBuilder` resolves Table 19-18 exactly once from the
canonical HT mode/header and produces the immutable generic descriptor. The
transmitter plan and `Ieee80211HtAlamoutiDecoder` consume the same value; neither
reconstructs a second table mapping.

For the first supported HT 1SS-to-2STS layout:

1. Validate the HT format, STBC indication, `NSS=1`, `NSTS=2`, at least two
   mapped Tx antennas, radio-owned `htRxStbc >= 1`, and a configured decoder.
2. Evaluate both mapped channel columns independently at the centers of the two
   OFDM symbols. Resolve every interferer's activity, plan segment, and STBC slot
   independently at both absolute times; an interferer may start between them.
3. Consume the shared descriptor whose exact Table 19-18 mapping, before
   normalization, gives slot `2m` as `[s0,-conj(s1)]` and slot `2m+1` as
   `[s1,conj(s0)]`, with both slot vectors multiplied by `alpha=1/sqrt(2)`.
4. Extend the descriptor/materializer boundary to build the augmented desired
   operator from slot-specific `H0,Q0` and `H1,Q1`, plus the complete augmented
   interferer/noise covariance. Retain the current constant-channel/covariance
   decoder overload only as a convenience wrapper for pure fixtures.
5. Apply the closed-form Alamouti decoder only when its equal-slot preconditions
   are proven. Otherwise pass the fully formed augmented operator/covariance to
   the generic factorization-based detector; no silent constant-slot fallback is
   allowed.
6. Return one recovered spatial-stream SINR. Unsupported HT layouts fail with a
   precise diagnostic; no scalar MRC fallback is allowed.

After the first layout passes, the same typed code descriptor and augmented
model may be extended to the remaining valid HT Table 19-18 layouts. VHT and HE
must use separate standard-revision adapters and gating rather than scattered
format checks in the generic processor.

## Time/frequency evaluation and scalar compatibility

Every exact strategy evaluation is a pure function of immutable context,
absolute time, and frequency. It consumes no RNG and mutates no selection or SIC
state. The initial receiver uses perfect instantaneous channel/covariance
knowledge at the evaluated point.

The current fixed channel-gain grid is insufficient by itself because antenna
choices and SIC order may change between nodes. The first runtime release
therefore freezes HT decoding on the actual OFDM resource lattice: one immutable
receiver result per modeled OFDM-symbol/subcarrier resource cell. Selection and
SIC order are piecewise constant inside that cell, so an unobserved crossing
between resource elements has no modeled meaning. This is both deterministic and
closer to the abstraction used by an OFDM receiver than continuously
interpolating a discrete detector choice.

A future continuous-time/frequency receiver may use deterministic adaptive
subdivision, but it must be described as a bounded-error numerical
approximation. Endpoint/midpoint sampling cannot prove that a black-box antenna
selector or SIC detector has no narrow hidden crossing. Do not claim continuous
crossing completeness without certified regional bounds or explicit root
enumeration from the snapshot/processor contracts.

`ISpatialSnir` exposes per-part/per-stream functions. Existing APIs still need a
scalar summary, so add an interface-typed mapping policy. The first policy is
`MinimumSpatialStreamSnirMapper`: the data value is the minimum required stream
SINR at each point, and the whole-reception summary also includes the legacy
header path. This is conservative integration behavior, not calibrated MIMO PER.
The IEEE error-model path must explicitly request this mapping; it must not
silently use stream 0 or average streams.

Sensitivity uses every unit-norm output weight's signal-only desired PSD for
every required stream; a reception passes only if all required outputs meet the
mode threshold. CCA and `SignalPowerInd` retain the separate aggregate physical
energy meaning defined above.

## Detailed runtime integration design and critical pseudocode

This section refines Phases 2, 3, 4, 6, and 7 below. It is the implementation
order for the remaining runtime work; the generic algebra, non-STBC strategies,
typed STBC descriptor, and pure Alamouti decoder already present in the working
tree are inputs to this work and must not be reimplemented in the medium.

### Decisions to freeze before runtime code changes

| Decision | First complete runtime policy |
|---|---|
| Receiver sampling domain | HT and legacy OFDM processing is evaluated on technology-supplied OFDM resource cells. The common medium obtains those cells through `IChannelMatrixReceiver`; it contains no IEEE subcarrier or symbol constants. |
| Cell value | Evaluate the exact channel, plans, covariance, and receiver strategy at the resource-cell center. Treat the resulting status, selected rows, and SIC order as constant over that cell. Plan/interferer boundaries split cells first. |
| Resource bound | Add `maximumMaterializedResourceCells` as a receiver/materializer safety limit. Freeze its documented default from HT20/HT40 maximum-duration benchmarks before enabling runtime use; exceeding it throws before allocation and never silently coarsens the grid. |
| Continuous receiver mode | Not enabled in the first release. If later enabled, use bounded deterministic refinement with an explicit nonconvergence failure and no claim that finite samples prove absence of hidden crossings. |
| Covariance | The aggregate `Rz` must be finite, Hermitian, and positive definite. Zero background PSD is accepted only if interference makes `Rz` positive definite; a zero or singular aggregate fails with a point-specific diagnostic. No diagonal loading or epsilon noise is invented. The legacy scalar path retains its existing zero-noise/infinite-SNIR behavior. |
| Strategy lifetime | Processor composition and every policy parameter are initialization-time constants. Runtime mutation is rejected rather than added implicitly to cache identity. |
| Interference cache | Derived values are revisioned per receiver/reception. A newly added or explicitly removed overlapping transmission invalidates only affected derived versions; physical reception data remains reusable. |
| Part-by-part reception | Keep a temporary initialization error for `channelMatrixReceptionProcessor` combined with `separateReceptionParts=true` until revisioned caches and single-draw part decisions are complete. Remove the error only after the late-interferer gates below pass. |
| Sensitivity | Acquisition uses structural/band checks and aggregate physical desired power. Final decoding additionally requires every materialized unit-normalized output stream to meet sensitivity. |
| Power semantics | Reception `getPower()` and `SignalPowerInd` mean aggregate physical desired power. Noise `getPower()` means physical CCA energy. Decoded desired/residual/noise PSDs exist only in `ISpatialSnir`. |
| Unsupported versus malformed | A valid but locally unsupported PPDU produces a PHY `UNSUPPORTED_RATE`/typed unsupported result and still contributes to CCA. Malformed metadata, invalid plan coverage, invalid dimensions, nonfinite math, or contradictory configuration throws before derived-cache publication. |

### Concrete change surface

| Area | Files and responsibility |
|---|---|
| Spatial-plan attachment | Extend existing `SpatialTransmissionPlan.{h,cc}` with complete-coverage and side-aware lookup; make `Ieee80211Transmission.{h,cc}` own `shared_ptr<const SpatialTransmissionPlan>` through `ISpatialTransmission`; build it once in `Ieee80211Transmitter.{h,cc}`. The `.msg` may expose a non-owning descriptor view, but must not serialize the shared pointer. |
| Physical reception | Refactor `DimensionalMediumAnalogModel.{h,cc}`, `ChannelMatrixReceptionAnalogModel.{h,cc}`, and `ChannelMatrixNoise.{h,cc}` so reception/noise retain immutable physical inputs and aggregate power, never receiver weights or decoded power. |
| Eager receiver output | Add `receiver/ChannelMatrixReceptionMaterializer.{h,cc}` and finish `IChannelMatrixReceiver`, processor, mapper, and `ISpatialSnir` contracts. Refactor `ChannelMatrixSnir.{h,cc}` into a self-contained immutable value with eager summaries. |
| Derived-cache revisions | Extend `ICommunicationCache.h`, `CommunicationCacheBase.{h,cc}`, all Map/Vector/Reference cache implementations, and `RadioMedium.cc` with per-reception interference revisions and ownership-safe invalidation/publication. |
| Sensitivity/part decisions | Refactor `ReceiverBase.{h,cc}`, override the spatial decision path in `Ieee80211Receiver.{h,cc}`, and change `Radio::continueReception()` to consume cached part decisions rather than invoking a second error decision. |
| HT on-air authority | Extend `Ieee80211PhyHeader.msg`, its serializer, `Ieee80211HtMode`, `Ieee80211ModeSet`, and new `Ieee80211PhyModeResolver`/`Ieee80211HtCapabilities` values. Never edit generated `_m.*`. |
| Capability exchange | Extend `linklayer/ieee80211/mgmt/Ieee80211MgmtFrame.msg`, `Ieee80211MgmtFrameSerializer.{h,cc}`, `Ieee80211MgmtBase.{h,cc}`, `Ieee80211MgmtAp.{h,cc}`, `Ieee80211MgmtSta.{h,cc}`, and `mib/Ieee80211Mib.{h,cc}`. Add a typed peer-capability-provider contract under `mac/contract`; `RateSelection` and `QosRateSelection` consume it without downcasting management modules. |
| Configuration migration | Add transmitter-local ordered antenna indices plus radio-local `htTxStbc=false` and `htRxStbc=0`; remove medium-owned `selectedTransmitAntenna` only in the same slice that migrates the showcase and focused tests. |

All named production paths are currently unsealed; recheck the authoritative
sealing list immediately before editing. The recursive seal under
`src/inet/common/packet/` must not be widened or bypassed.

### 1. Make every attached spatial plan total and side-aware

The current pure `SpatialTransmissionPlan` rejects overlaps but permits gaps and
has only right-limit half-open lookup. Add an attachment gate so no runtime
consumer can observe an uncovered PPDU offset. Preserve the generic ability to
construct a partial plan for isolated unit tests if useful, but an
`ISpatialTransmission` must carry a plan that passes complete coverage.

```cpp
void SpatialTransmissionPlan::validateCompleteCoverage(simtime_t duration) const
{
    require(duration > 0);
    require(!segments.empty());
    require(segments.front().getStartOffset() == SIMTIME_ZERO);

    for (int i = 1; i < segments.size(); ++i)
        require(segments[i - 1].getEndOffset() ==
                segments[i].getStartOffset());

    require(segments.back().getEndOffset() == duration);
}

const Segment& segmentAt(simtime_t offset, BoundarySide side) const
{
    if (side == RIGHT_LIMIT) {
        require(0 <= offset && offset < duration);
        return segment whose [start,end) contains offset;
    }
    else {
        require(0 < offset && offset <= duration);
        return segment whose (start,end] contains offset;
    }
}
```

Do not approximate a boundary with a floating epsilon. Simulation-time lookup
uses the explicit side. A resource cell at the final PPDU endpoint requests the
left limit; a query outside `[start,end)` returns zero without asking the plan
for a segment.

Extend each segment with an immutable, technology-neutral mapping descriptor
whose `resolve(basebandFrequency)` returns `Q(f)`. The IEEE plan builder supplies
direct antenna mapping and CSD delays; common code sees only the descriptor and
does not contain Table 19-10 values.

### 2. Build and publish only physical reception data before covariance exists

`computeReception()` runs before interference/noise/SNIR is available. Its
matrix path must therefore compute only receiver-independent physical values:

```cpp
const IReception *computeReception(rxRadio, transmission, arrival)
{
    auto spatial = dynamic_cast<const ISpatialTransmission *>(transmission);
    if (!channelModel || !spatial)
        return computeUnchangedLegacyReception(...);

    auto snapshot = channelModel->computeChannel(rxRadio, transmission, arrival);
    auto plan = spatial->getSpatialTransmissionPlan();
    plan->validateCompleteCoverage(transmission->getDuration());
    validateSnapshotAndAntennaDimensions(snapshot, plan, rxRadio, transmission);

    auto largeScalePsd = computeReceptionPower(rxRadio, transmission, arrival);
    auto physicalPsd = eagerlyMaterializePhysicalPower(
        snapshot, plan, largeScalePsd, arrival);

    // No selected rows, weights, Rz, or decoded stream power here.
    auto analog = new ChannelMatrixReceptionAnalogModel(
        durations, band, snapshot, plan, largeScalePsd,
        physicalPsd, /* separately named CCA accessor */ physicalPsd);
    return new Reception(..., analog);
}
```

At each physical-power evaluation point use:

```text
Btx = Q Csts Q^H
physicalDesiredPsd = P * trace(H Btx H^H)
```

For the compatibility plan `Q=e0`, `Csts=[1]`, this is exactly
`P*sum_rx |H[rx,0]|^2`, the delivered selected-column MRC gain. For multiple
streams or STBC it observes total radiated energy without multiplying transmit
power.

`ChannelMatrixNoise::getPower()` remains:

```text
ccaPsd = backgroundPsd counted once
       + sum_j physicalAggregatePsd(interferer_j)
```

Replace borrowed `vector<const IReception *>` state with immutable interferer
descriptors containing transmission ID, time/frequency bounds, snapshot, shared
plan, and large-scale PSD function. The eager materializer may use those
descriptors while constructing a result; later numeric queries must not
dereference a foreign reception-cache entry.

### 3. Evaluate one exact covariance/detection sample

The materializer is technology-neutral. The receiver capability supplies
required resource cells, the initialization-frozen processor, and the scalar
mapping policy.

```cpp
ExactSpatialSample evaluateExact(
    const PhysicalReceptionInput& desired,
    const vector<PhysicalInterfererInput>& sortedInterferers,
    const IChannelMatrixReceptionProcessor& processor,
    simtime_t t, Hz f, SignalPart part)
{
    auto desiredSegment = desired.plan->segmentAt(
        t - desired.receptionStart, RIGHT_LIMIT);
    ChannelMatrixReceptionContext::Signal desiredSignal(
        desired.snapshot->getResponse(t, f),
        desiredSegment.resolveAt(f),
        desired.largeScalePsd->getValue({t, f}));

    vector<ChannelMatrixReceptionContext::Signal> interferers;
    for (const auto& interferer : sortedInterferers) {
        if (!interferer.isActiveHalfOpen(t, f))
            continue;
        auto segment = interferer.plan->segmentAt(
            t - interferer.receptionStart, RIGHT_LIMIT);
        interferers.emplace_back(
            interferer.snapshot->getResponse(t, f),
            segment.resolveAt(f),
            interferer.largeScalePsd->getValue({t, f}));
    }

    WpHz n0 = backgroundPsd->getValue({t, f});
    ComplexMatrix Rbackground = identity(numberOfRxRows) * n0.get();
    ChannelMatrixReceptionContext context(
        desiredSignal, interferers, Rbackground, t, f, part);

    // Context forms A and Rz; the processor applies subset + strategy.
    ChannelMatrixDetectionResult result = processor.compute(context);
    return ExactSpatialSample::copyOf(context, result);
}
```

For an STBC desired segment, do not call the single-point overload. Build one
augmented context for the complete code pair:

```cpp
ExactSpatialSample evaluateExactStbcPair(inputs, CodePair pair, Hz f)
{
    simtime_t t0 = pair.slot0.centerTime();
    simtime_t t1 = pair.slot1.centerTime();
    auto d0 = resolveDesiredSignal(inputs.desired, t0, f);
    auto d1 = resolveDesiredSignal(inputs.desired, t1, f);
    require(d0.codeDescriptor == d1.codeDescriptor);

    ComplexMatrix Abar = d0.codeDescriptor->buildAugmentedEffectiveChannel(
        d0.response, d0.mapping, d0.psd,
        d1.response, d1.mapping, d1.psd);

    ComplexMatrix Rbar = blockDiagonal(
        backgroundCovarianceAt(t0, f),
        conjugate(backgroundCovarianceAt(t1, f)));

    for (const auto& interferer : inputs.interferersSortedByTransmissionId) {
        auto i0 = resolveOptionalSignal(interferer, t0, f); // inactive is zero
        auto i1 = resolveOptionalSignal(interferer, t1, f);
        Rbar += buildAugmentedInterfererCovariance(i0, i1, pair, f);
        // The interferer's descriptor supplies cross-slot blocks when the two
        // observations belong to one correlated STBC codeword. Independent
        // ordinary symbols produce zero cross blocks.
    }

    AugmentedChannelMatrixReceptionContext context(
        Abar, Rbar, pair.observationCoordinates(), pair.part());
    return processor.computeSpaceTimeBlockCode(context);
}
```

Extend `SpaceTimeCodeDescriptor` and `Ieee80211HtAlamoutiDecoder` with this
slot-specific augmented-context entry point. The current overload taking one
channel and one covariance becomes a checked wrapper that supplies identical
slot values. It must not be the runtime interface. An interferer beginning
between `t0` and `t1` therefore contributes zero in the first covariance block
and physical energy in the second; an STBC interferer aligned differently uses
its own absolute slot indices and descriptor cross terms.

The context computes, in fixed transmission-ID order:

```text
A  = sqrt(Pd) * Hd * Qd * diag(sqrt(pd))
Rz = N0 I + sum_j Pj Hj Qj Csts,j Qj^H Hj^H
```

STBC replaces the ordinary `A` and `Rz` with the descriptor-built augmented
two-slot values; it does not pass the two STS columns to ZF/MMSE as independent
spatial streams. Validate `Rz` before the strategy call and report the exact
transmission, receiver, time, and frequency in any covariance diagnostic.

### 4. Eagerly materialize immutable resource-cell results

First collect all discrete boundaries before evaluation:

```cpp
MaterializationGrid buildGrid(desired, interferers, receiverCapability)
{
    timeCuts = {
        desired reception start/end,
        coarse PPDU part boundaries,
        desired spatial-plan boundaries,
        desired OFDM symbol boundaries and atomic STBC code-pair resources
    };

    for (interferer sorted by transmission ID) {
        insert clipped interferer arrival start/end;
        insert clipped interferer plan and STBC slot boundaries;
    }

    frequencyCells = receiverCapability.getResourceFrequencyCells(desired);
    require cells are ordered, nonoverlapping, inside receiver/signal band;
    split non-STBC time cells at every timeCut;
    // Never split a desired STBC pair. Resolve any interferer boundary inside
    // it independently at the two slot centers in evaluateExactStbcPair().
    require checkedCellCount <= maximumMaterializedResourceCells;
    return lexicographically ordered time-major/frequency-major cells;
}
```

Then materialize exactly once:

```cpp
unique_ptr<const MaterializedSpatialReception> materialize(inputs, processor)
{
    vector<MaterializedSpatialCell> cells;
    for (const auto& cell : buildGrid(inputs)) {
        Hz f = cell.centerFrequency();
        auto sample = cell.isSpaceTimeCodePair() ?
            evaluateExactStbcPair(inputs, cell.codePair(), f) :
            evaluateExact(inputs, processor,
                cell.centerTime(), f, cell.part());

        cells.emplace_back(cell.bounds(), sample.status,
            sample.selectedRows, sample.detectionOrder,
            sample.desiredPsd, sample.residualPsd,
            sample.projectedNoisePsd, sample.sinr);
    }

    auto result = make_unique<MaterializedSpatialReception>(move(cells));
    result->computeAndStoreScalarMappingAndExtrema();
    return result; // no exact evaluator, module, cache, or reception pointer
}
```

The immutable piecewise functions return zero outside the declared time/band
domain. Within a cell they return the stored constant. `partition()` enumerates
stored cells; `getMin()`, `getMax()`, and `getMean()` are constructor-computed
values. A non-success detector status stores finite zero PSD/SINR outputs and
maps to scalar SNIR zero.

`ChannelMatrixSnir` may still expose the non-owning reception/noise metadata
pointers required by the existing `ISnir` contract; the enclosing cache version
owns those objects for longer than the SNIR. Its numeric payload and every
summary must be the self-contained `MaterializedSpatialReception` and must never
dereference those metadata pointers during evaluation.

If a later feature explicitly enables continuous processing, use nine
deterministic probes per rectangle, split on status/selected-row/SIC-order
changes or a frozen absolute/relative output error, split time before frequency
on ties, and throw at maximum depth. Keep that implementation behind a separate
typename/configuration gate and label it bounded approximation.

### 5. Make derived cache publication interference-revision safe

The existing cache key `(receiver, transmission)` is adequate for immutable
physical reception data but not for a derived interference graph. Add a
monotonic revision to each receiver/reception entry. A derived version owns a
self-consistent interference, noise, SNIR, part decisions, and final result:

```cpp
struct DerivedReceptionVersion {
    uint64_t interferenceRevision;
    unique_ptr<const IInterference> interference;
    unique_ptr<const INoise> noise;
    unique_ptr<const ISnir> snir;
    array<shared_ptr<const IReceptionDecision>, NUM_PARTS> decisions;
    unique_ptr<const IReceptionResult> result;
};

struct ReceptionCacheEntry {
    // Existing physical arrival/listening/reception/signal values.
    uint64_t currentInterferenceRevision = 0;
    unique_ptr<DerivedReceptionVersion> currentDerived;
    vector<unique_ptr<DerivedReceptionVersion>> retiredDerived;
};
```

Retain retired immutable versions until the reception cache entry is destroyed.
This is intentionally more conservative than deleting raw objects during
invalidation: an already returned decision or background computation cannot be
left with dangling pointers. The number of versions is bounded by transmissions
that begin while the reception is live. A new version shares completed,
unaffected part decisions with the retired version; it never moves them out of
or mutates the older version.

After installing a new arrival/interval for a receiver, query overlapping
existing desired intervals in deterministic transmission-ID order:

```cpp
void RadioMedium::addTransmission(txRadio, newTx)
{
    communicationCache->addTransmission(newTx);
    for (auto receiver : receiversInStableIdOrder()) {
        auto arrival = installArrivalIntervalAndListening(receiver, newTx);
        auto desireds = overlappingCachedTransmissions(receiver, arrival);
        sortByTransmissionId(desireds);

        for (auto desired : desireds)
            if (desired != newTx &&
                isInterferingTransmission(newTx, getReception(receiver, desired)))
                communicationCache->advanceInterferenceRevision(
                    receiver, desired, arrival->getStartTime(), arrival->getEndTime());
    }
    sendSignalCopies(...);
}
```

Explicit early removal performs the symmetric invalidation before deleting the
interferer. Normal expiry must assert that it cannot overlap an unfinished
reception.

Every derived getter captures and publishes one revision:

```cpp
const ISnir *RadioMedium::getSNIR(receiver, transmission) const
{
    for (;;) {
        uint64_t revision = cache->getInterferenceRevision(receiver, transmission);
        if (auto value = cache->getCachedSNIR(receiver, transmission, revision))
            return value;

        auto immutableInputs = buildInputs(receiver, transmission, revision);
        unique_ptr<const ISnir> candidate =
            analogModel->computeSNIR(immutableInputs);

        if (cache->tryPublishSNIR(
                receiver, transmission, revision, candidate.get()))
            return candidate.release();
        // An overlapping transmission changed the revision. Discard and retry.
    }
}
```

Apply the same captured-revision rule to interference, noise, part decisions,
and the final result so values from two revisions cannot be mixed. Cache access
and publication remain on the OMNeT++ simulation thread. If evaluation is later
offloaded, a worker receives only a reference-counted immutable input bundle;
the simulation thread publishes its candidate after the revision check.

`ReferenceCommunicationCache` keeps its intentional always-miss behavior but
must still reject stale publication and own accepted/retired candidates.

For `separateReceptionParts=true`, complete all of these before removing the
temporary gate:

1. Invalidate only cached decisions for parts whose intervals overlap the new
   interferer; carry completed unaffected part decisions into the new revision.
2. Make `Radio::continueReception()` obtain the previous-part result with
   `getReceptionDecision()`; never call the direct path that can draw PER twice.
3. Make `RadioMedium::isReceptionSuccessful()` itself delegate to the
   revision-current cached decision so other callers cannot bypass the
   single-draw rule.
4. Construct the final result from the actual cached preamble/header/data
   decisions instead of recomputing `SIGNAL_PART_WHOLE`.
5. Test Map, Vector, and Reference caches with an interferer that starts after
   preamble materialization and overlaps only Data.

### 6. Separate acquisition, decoded sensitivity, CCA, and indications

First separate medium delivery from decoder support. `RadioMedium` uses the
transmission-level overload of `computeIsReceptionPossible()` for
`listeningFilter`; on the matrix IEEE path that overload must mean only physical
technology/band relevance. It must not inspect local MCS, Rx-STBC, decoder, or
detector support:

```cpp
bool Ieee80211Receiver::computeIsReceptionPossible(
    listening, transmission) const
{
    auto ieee = dynamic_cast<const Ieee80211Transmission *>(transmission);
    return ieee != nullptr && bandsPhysicallyOverlap(listening, *ieee);
    // Receiver-local canonical/support validation occurs only after delivery.
}
```

A valid physical PPDU with `UNSUPPORTED_RATE` must still receive arrival events
and remain in interference/CCA calculations when `listeningFilter=true`. The
receiver may decline acquisition/decoding, but the medium cannot filter its
energy because a local decoder is absent.

Add a protected helper to `ReceiverBase` that performs only the arbitration
portion after feasibility is already known:

```cpp
bool computeIsReceptionAttemptedAfterPossibility(
    listening, reception, part, interference) const;

bool computeIsReceptionAttempted(...) const
{
    return computeIsReceptionPossible(listening, reception, part) &&
           computeIsReceptionAttemptedAfterPossibility(
               listening, reception, part, interference);
}
```

Also refactor the scalar threshold helper so it asks `ISpatialSnir` for
`getMinimum(part)` or `getMean(part)` when available; the current
`SnirReceiverBase` ignores its `part` argument and uses whole-reception extrema.
The error model already receives the requested part, but the threshold gate must
use the same part before the one cached PER draw.

The spatial IEEE decision path then orders its checks explicitly:

```cpp
const IReceptionDecision *Ieee80211Receiver::computeReceptionDecision(
    listening, reception, part, interference, snir) const
{
    auto spatial = dynamic_cast<const ISpatialSnir *>(snir);
    if (!spatial)
        return FlatReceiverBase::computeReceptionDecision(...); // unchanged

    bool structural = isCanonicalModeBandAndLayoutSupported(reception, part);
    bool decodedSensitivity = structural &&
        spatial->allRequiredOutputPowersMeet(part, sensitivity);
    bool possible = structural && decodedSensitivity;
    bool attempted = possible &&
        computeIsReceptionAttemptedAfterPossibility(
            listening, reception, part, interference);
    bool successful = attempted &&
        computeIsReceptionSuccessful(
            listening, reception, part, interference, snir);
    return new ReceptionDecision(
        reception, part, possible, attempted, successful);
}
```

At initial part start, acquisition uses canonical mode/layout/band support,
aggregate physical desired power, and radio occupancy. It must not recursively
run a processor from `computeIsReceptionPossible(reception,part)`. At the final
decision, `allRequiredOutputPowersMeet()` integrates each stream's stored desired
PSD over frequency, takes the minimum over the requested part, and requires all
streams to meet the existing mode sensitivity.

Refactor the matrix `SignalPowerInd` path to integrate
`ChannelMatrixReceptionAnalogModel::getPower()` directly. Do not obtain the tag
from `computeNoise(reception,noise)`, which is a signal-plus-noise operation in
the current base implementation. Receiver strategy changes, decoded
interference nulling, and decode failure must not change `SignalPowerInd` or CCA.

### 7. Complete HT-SIG representation and canonical mode resolution

Add the complete 48-bit typed HT-SIG representation. In transmitted bit order:

```text
bits  0.. 6  MCS
bit       7  CBW
bits  8..23  HT Length
bit      24  Smoothing
bit      25  Not Sounding
bit      26  Reserved (must be 1)
bit      27  Aggregation
bits 28..29  STBC
bit      30  FEC Coding
bit      31  Short GI
bits 32..33  NESS
bits 34..41  CRC
bits 42..47  Tail (zero)
```

Serialize HT-SIG1 before HT-SIG2 and every field LSB-first. The CRC protects
bits 0--33, uses `D^8 + D^2 + D + 1`, complements the result, and transmits
`c7` first. Keep the independent `8f 64 00 07 54 00` vector from Clause
19.3.9.4.4 as the bit-exact oracle; serializer round-trip is not its own proof.

Canonicalization and local support are two different pure operations. The first
is receiver-independent and may be stored on one shared/broadcast transmission:

```cpp
HtCanonicalization canonicalizeHtPpdu(
    header, preambleFormat, transmitChannelContext)
{
    if (!header.hasExactly48Bits() || !verifyHtSigCrc(header))
        return FORMAT_VIOLATION;
    if (!header.reserved || header.tail != 0 || header.stbc == 3 ||
        header.mcs >= 77)
        return RESERVED_HT_SIG; // reported as UnsupportedRate

    auto mcs = htMcsRegistry.findStandardMetadata(header.mcs);
    if (!mcs)
        return RESERVED_HT_SIG;

    int nss = mcs->numberOfSpatialStreams; // explicit table metadata
    if (!isLegalTable19_12Pair(nss, header.stbc))
        return RESERVED_HT_SIG;
    int nsts = nss + header.stbc;

    if (nsts + header.ness > 4 || computeHtLtfCount(nsts, header.ness) > 5)
        return RESERVED_HT_SIG;

    return SUCCESS(CanonicalHtPpduDescription(
        header, preambleFormat, transmitChannelContext, nss, nsts));
}

HtReceiveSupport validateHtReceiveSupport(
    const CanonicalHtPpduDescription& ppdu,
    const ReceiveChannelContext& receiveChannel,
    const Ieee80211HtCapabilities& localCaps,
    const IChannelMatrixReceptionProcessor& processor)
{
    if (!receiveChannel.isTunedFor(ppdu) ||
        !localCaps.supportsMcsAndWidth(ppdu.mcs, ppdu.bandwidth))
        return UNSUPPORTED_RATE;

    // Bounded first release: HT mixed, EQM MCS 0..31, BCC, NESS=0.
    if (ppdu.preambleFormat != HT_MIXED || ppdu.mcs > 31 ||
        ppdu.fecCoding != BCC || ppdu.ness != 0)
        return UNSUPPORTED_RATE;

    if (ppdu.stbc != 0 &&
        (localCaps.rxStbc < ppdu.nss ||
         !processor.hasDecoder(ppdu.nss, ppdu.nsts)))
        return UNSUPPORTED_RATE;

    if (ppdu.stbc == 0 && ppdu.nss > 1 &&
        !processor.hasSpatialStreamDetector())
        return UNSUPPORTED_RATE;

    return SUPPORTED(canonicalModeFrom(ppdu));
}
```

Make NSS explicit metadata in the HT MCS registry. Do not infer it by counting
non-null modulation pointers: MCS 32 is one spatial stream despite the current
descriptor shape. The bounded release supports EQM MCS 0--31 and reports MCS 32
and UEQM 33--76 as `UNSUPPORTED_RATE` until their special mappings exist.

Use every Table 19-12 row, not only `NSTS=NSS+STBC`:

| NSS | Legal STBC values | Resulting NSTS |
|---:|---:|---:|
| 1 | 0, 1 | 1, 2 |
| 2 | 0, 1, 2 | 2, 3, 4 |
| 3 | 0, 1 | 3, 4 |
| 4 | 0 | 4 |

The receiver-independent canonical-description/mode cache identity includes at least band, preamble format,
bandwidth, MCS, guard interval, FEC, and STBC. Optional sender-selected mode
metadata may be compared for consistency, but it is never receive authority.
Add a two-receiver fixture in which one shared transmission is supported by one
radio and returns `UNSUPPORTED_RATE` at another because their `htRxStbc` or
Supported MCS Sets differ; neither receiver may mutate the transmission.

### 8. Complete local and peer capability gating

Construct one immutable local `Ieee80211HtCapabilities` in
`Ieee80211Radio` from validated NED parameters:

```text
htTxStbc = false by default
htRxStbc = 0 by default, legal range 0..3
supportedMcsSet and supported channel widths from the authoritative mode set
```

Management inserts this same value in Beacon, Probe Request/Response,
Association Request/Response, and Reassociation Request/Response HT Capabilities elements.
AP/STA management or the MIB owns the last accepted immutable capability value
per peer MAC address and clears it on disassociation/deauthentication. Rate
selection queries a typed read-only peer-capability provider.

Normal unicast STBC transmission is legal only when:

```cpp
bool canTransmitHtStbc(local, peer, mode, txAntennaCount)
{
    return local.htTxStbc &&
           mode.isHtMixed() && mode.stbc != 0 &&
           isLegalTable19_12Pair(mode.nss, mode.stbc) &&
           mode.nsts <= txAntennaCount &&
           hasExactEncoderAndMapping(mode.nss, mode.nsts) &&
           peer != nullptr &&
           peer->supportsMcsAndWidth(mode.mcs, mode.bandwidth) &&
           peer->rxStbc >= mode.nss;
}
```

The transmitter performs every gate and builds the one authoritative plan
before the medium can see the transmission:

```cpp
const ITransmission *Ieee80211Transmitter::createTransmission(...)
{
    auto requestedMode = computeTransmissionMode(packet);
    auto peer = peerCapabilityProvider->find(destinationAddress(packet));

    if (requestedMode->getStbc() != 0 &&
        !canTransmitHtStbc(localCapabilities, peer,
            *requestedMode, transmitter->getAntenna()->getNumAntennas()))
        throw cRuntimeError("Requested HT STBC mode is not legal for peer");

    auto header = buildCompleteHtSig(*requestedMode, packetLength);
    auto canonical = phyModeResolver->canonicalizeHtPpdu(
        header, preambleFormat, channel);
    require(canonical.matches(*requestedMode));

    shared_ptr<const SpatialTransmissionPlan> plan = spatialPlanBuilder->build(
        canonical, configuredTransmitAntennaIndices, computedDurations);
    plan->validateCompleteCoverage(computedDuration);

    return new Ieee80211Transmission(
        ..., canonical.mode, channel, plan);
    // Only the caller may now pass this fully validated value to RadioMedium.
}
```

Unknown peer state selects a legal non-STBC/basic mode. An explicit illegal
STBC request throws before transmission construction; it is never silently
downgraded. Keep group-addressed STBC disabled in the first release because the
model lacks a reliable complete intended-recipient capability set.

Receive dispatch after canonical resolution is:

```cpp
if (stbc == 0 && nss == 1)
    require(singleStreamCombiner);
else if (stbc == 0)
    require(spatialStreamDetector);
else {
    require(local.htRxStbc >= nss);
    require(exactDescriptorAndDecoder(nss, nsts));
    dispatchStbcDecoder(); // never ZF/MMSE over STS columns
}
```

The first decoder supports only `(NSS,NSTS,STBC)=(1,2,1)`. Other standard-valid
layouts are `UNSUPPORTED_RATE` until an exact descriptor/decoder is installed;
they are not malformed. Active receive chains below NSS produce detector status
`UNDERDETERMINED`. Any unsupported or failed decoder still contributes full
physical CCA energy.

Receive-only STBC integration may land before management capability exchange,
because it needs only the local advertised Rx-STBC value and an installed
decoder. Ordinary rate selection must keep STBC disabled until the peer path and
`canTransmitHtStbc()` gates pass.

### 9. Failure and status taxonomy

Keep these layers separate in code, logs, and tests:

| Layer | Examples | Result |
|---|---|---|
| Configuration/programming | Invalid NED range, duplicate antenna index, plan gap/overlap, header/mode/plan contradiction, nonfinite mapping, capability claims without installed support | Throw before medium or derived-cache publication. |
| PHY/header | Bad/incomplete HT-SIG CRC, reserved bit/tail/STBC/MCS combination, locally unsupported MCS/width/NESS/STBC | `FORMAT_VIOLATION`, `RESERVED_HT_SIG`, or `UNSUPPORTED_RATE`; no decode, CCA preserved. |
| Materialization | Non-HPD `Rz`, invalid function value, missing resource cells, stale revision that repeatedly fails publication | Point-specific runtime error; never publish a partial grid. A stale candidate alone is discarded and retried. |
| Detector | `UNSUPPORTED_LAYOUT`, `UNDERDETERMINED`, `RANK_DEFICIENT`, `ILL_CONDITIONED` | Immutable finite zero-output result, scalar compatibility SNIR zero, CCA preserved. |
| Physical zero channel | Valid dimensions and HPD `Rz`, but no desired response | Successful computation with zero desired PSD/SINR or the detector's typed rank result, never NaN. |

### 10. Runtime implementation order and focused gates

Implement in slices that never leave sensitivity or CCA with mixed meanings:

1. **Plan-totality slice.** Add complete coverage, side-aware lookup,
   frequency-dependent mapping descriptor, attachment ownership, and transmitter
   antenna-list migration. Gate: plan gaps/endpoints/dimensions fail before
   `RadioMedium::addTransmission()`.
2. **Header-authority slice.** Complete HT-SIG, CRC, MCS metadata, Table 19-12,
   canonical resolver, local `htTxStbc`/`htRxStbc`, and receive-only support.
   Keep production STBC Tx disabled. Gate: bit-exact header and all resolver
   invalid/unsupported cases.
3. **Physical-power slice.** Change reception/noise to physical aggregate
   semantics and change `SignalPowerInd` in the same patch. Leave the default
   processor fixed to compatibility MRC. Gate: `Q=e0` yields the delivered gain,
   sensitivity outcome, CCA, and tag.
4. **Eager-materializer slice.** Add resource cells, copied interferer inputs,
   exact covariance/context construction, immutable output functions, and eager
   summaries. Gate: construction performs all evaluator calls; later shuffled
   queries perform none and are bit-identical.
5. **Sensitivity/composition slice.** Add processor NED composition, decoded
   sensitivity decision, scalar mapper, and SC/maximum-SINR/ZF/MMSE/SIC runtime
   dispatch. Gate: analytical strategy fixtures plus physical CCA invariance.
6. **Revisioned-cache slice.** Add overlap invalidation, version-safe
   publication, retained immutable versions, cached per-part decisions, and
   Map/Vector/Reference coverage. Remove the `separateReceptionParts` gate only
   here.
7. **STBC runtime slice.** Attach the shared Table 19-18 descriptor to the HT
   plan, extend the decoder with a slot-specific augmented-context entry point,
   materialize both slot responses and the full two-slot covariance, and
   dispatch the Alamouti decoder. Gate: exact one/two-Rx fixtures, colored
   covariance, an interferer beginning between slots, differently aligned STBC
   interference, power conservation, local capability rejection, and unchanged
   `STBC=0`.
8. **Peer-capability slice.** Add HT Capabilities wire representation,
   management ownership/lifecycle, provider contract, and rate-selection
   filtering. Enable ordinary unicast STBC only after negotiation tests pass.
9. **Compatibility/review slice.** Run only directly mapped debug tests,
   feature-off build, architecture checks, and complete general/WLAN semantic
   review. Do not update fingerprints without explicit approval.

Add these focused tests beyond the pure strategy files already listed:

| Test | Critical runtime oracle |
|---|---|
| `ChannelMatrixReceptionMaterializer_1.test` | Eagerness counter, resource cells, plan/interferer boundaries, immutable copied inputs, query-order identity, zero outside domain. |
| `ChannelMatrixCovarianceMaterializer_1.test` | Direct `A`/`Rz` formula, canonical interferer order, HPD/zero-noise policy, selected-row reduction, scaling invariance. |
| `ChannelMatrixSnir_1.test` | Per-part/per-stream functions, eager min/max/mean, finite failure mapping, scalar minimum mapper. |
| `ChannelMatrixSensitivity_1.test` | Aggregate acquisition versus all-output decode sensitivity and default MRC compatibility. |
| `RadioMediumInterferenceRevision_1.test` | Late/removed interferer, stale publication retry, retained old result, Map/Vector/Reference parity, single PER draw. |
| `Ieee80211HtAlamoutiRuntimeContext_1.test` | Slot-specific `H0/H1`, `R0/R1`, full augmented covariance, an interferer beginning between slots, and differently aligned STBC interference. |
| `Ieee80211PhyHeaderSerializer_1.test` | Every HT-SIG bit, independent six-byte vector, CRC/reserved/tail/STBC rejection. |
| `Ieee80211PhyModeResolver_1.test` | MCS-to-NSS including MCS32 trap, every Table 19-12 row, NESS/LTF bounds, canonical cache identity. |
| `Ieee80211HtCapabilityGating_1.test` | Local Tx/Rx, peer Rx-STBC/MCS/width, unknown/group peer behavior, explicit request rejection. |
| `Ieee80211MgmtHtCapabilitiesSerializer_1.test` | HT Capabilities and Supported MCS Set fixed-byte serialization plus required Beacon, Association, Reassociation, Probe Request, and Probe Response carriage. |
| `Ieee80211HtCapabilityNegotiation_1.test` | Beacon/probe/association state ownership, replacement/clear, rate-selection enable/block, and two receivers with different local support for one PPDU. |
| `Ieee80211MimoReceiver_1.test` | Strategy runtime dispatch, legacy header/ACK, CCA invariance, local STBC support, decoded sensitivity, and `listeningFilter=true` delivery of unsupported physical PPDUs. |

The decisive late-interferer fixture is:

```text
desired PPDU: preamble/header/data, separateReceptionParts=true
1. cache preamble decision and SNIR at revision R
2. add interferer after preamble; it overlaps only Data
3. require Data interference/noise/SNIR at revision R+1
4. require cached preamble decision unchanged and no second PER draw
5. require final result to contain the actual cached part decisions
```

For physical/decoded separation, keep the analytical default fixture
`H=[3+4j;12j]`, `Q=e0`: aggregate desired and CCA PSD are `169P`, and with
white `N0 I` the compatibility MRC SINR is `169P/N0`. Place the sensitivity
threshold between a selected-branch value and `169P`; the result must follow the
documented acquisition/decoded policy, not whichever scalar function happens to
be stored in the reception object.

## Implementation phases and acceptance gates

### Phase 0 -- freeze semantics and migration

Before source edits:

1. Recheck sealing for every exact target.
2. Record the IEEE revision, clauses, tables, and corpus chunks above in the
   HT-SIG, mapping, training, and STBC decision points or focused tests.
3. Freeze the initial scope: local perfect-CSI subset selection, direct HT
   mapping, equal stream power, HT 1SS-to-2STS STBC, and perfect-cancellation
   MMSE-SIC.
4. Freeze the rank threshold, OFDM resource-cell semantics, checked cell-count
   safety limit, and scalar stream-SNIR mapping with deterministic unit fixtures
   and HT20/HT40 maximum-duration benchmarks. If the optional continuous
   materializer is later implemented, separately freeze its interpolation
   tolerance/depth and label it a bounded approximation.
5. Document the `selectedTransmitAntenna` configuration migration.
6. Confirm that all strategy choices are initialization-time constants; otherwise
   design explicit medium-cache invalidation before implementation.
7. Freeze the standard's Clause 19.3.9.4.4 example as the independent bit-exact
   HT-SIG oracle. Packing its LSB-first fields and transmitted `C7`-first CRC
   yields bytes `8f 64 00 07 54 00` (MCS 15, CBW 40 MHz, HT Length 100,
   Smoothing/Not-Sounding/Reserved set, STBC/FEC/Short-GI/NESS zero, CRC
   `10101000`, zero tail). Do not use serializer round-trip as its own oracle.
8. Freeze HT subpart boundaries, Table 19-10 CSD values, Tables 19-13--19-14
   HT-LTF counts/durations, and the separation of receive-supported modes from
   rate-selectable modes.

### Phase 1 -- generic algebra and immutable receiver values

Implement `ChannelMatrixAlgebra`, reception context, detection result, and
`ISpatialSnir` without changing runtime behavior.

Acceptance:

- checked complex multiplication/conjugate transpose and Hermitian solves match
  analytical fixtures;
- invalid dimensions, NaN/Inf, non-Hermitian covariance, and invalid receiver
  values fail at construction;
- rank/condition classification is deterministic immediately above and below
  the frozen threshold;
- result values are immutable and preserve unit-normalized output weights;
- existing `ChannelMatrixSnapshot` and TGn matrix tests remain unchanged.

### Phase 2 -- spatial transmission plan and compatibility migration

Add the generic non-STBC `SpatialTransmissionPlan`, attach the segment-specific
plan to IEEE transmissions, move transmit-column ownership from the medium to
the transmitter, and implement complete HT-SIG representation/mode
reconstruction before permitting a multi-stream receive path.

Acceptance:

- the six HT-SIG octets match the independent bit-exact oracle; CRC corruption,
  reserved/tail violations, and invalid MCS/STBC/NESS combinations are rejected;
- NSS is derived from MCS, NSTS is derived from NSS+STBC, and the received header
  is authoritative if sender-side metadata is absent; conflicting sender
  metadata is rejected before reception caching;
- receive-supported modes and rate-selectable modes are separate; explicit
  receive-only STBC does not become a transmit-rate candidate;
- MCS 32 is explicitly one spatial stream but remains unsupported in the
  bounded direct-EQM runtime, rather than being misclassified by modulation
  pointer count;
- HT MCS 0--31 direct-mapped plans carry authoritative NSS/NSTS, valid dimensions,
  equal source fractions, and unit-trace resolved STS covariance;
- every attached plan covers exactly `[0,PPDU duration)` without a gap or
  overlap and uses explicit left/right boundary lookup;
- Table 19-10 CSD phases, HT-STF/HT-LTF/Data segment boundaries, and
  NSTS/NESS-dependent HT-LTF counts and 4 us duration match exact fixtures;
- the default one-stream plan uses transmit antenna 0 and reproduces the old
  selected-column path without retaining two authoritative parameters.

### Phase 3 -- preserve matrices and eagerly materialize covariance

Refactor matrix reception/noise/SNIR values and the dimensional medium so no
receiver matrix is scalarized before `computeSNIR()`.

Acceptance:

- desired and every overlapping interferer retain snapshot, large-scale PSD,
  and spatial plan;
- `Rz` matches hand-computed diagonal and correlated-interference fixtures;
- every OFDM resource cell is materialized before cache publication, later
  function queries invoke no snapshot, receiver module, or exact evaluator, and
  shuffled query order is bit-identical;
- the current overlapping-interference exception is removed;
- CCA still sees raw interferer energy even when the decoder nulls it;
- no-channel dimensional behavior takes the original path;
- one-stream, no-interference default MRC remains numerically identical.

### Phase 4 -- receiver-local composition and one-stream strategies

Add the generic receiver capability, compound processor, antenna-selection
policies, selection combiner, maximum-SINR combiner, and MRC compatibility
policy. Configure them under `Ieee80211Receiver`, not the medium.

Acceptance:

- fixed and optimal receive subsets validate dimensions and tie-break
  lexicographically;
- a 4-Rx, two-active-chain fixture enumerates all six subsets, and active-chain
  counts smaller than a detector's required NSS produce a typed undecodable
  result rather than silently widening the subset;
- selection combining uses branch SINR rather than branch power;
- maximum-SINR uses the full covariance and reduces to MRC for `Rz=N0 I`;
- an orthogonal interferer is nulled in decoded SINR while remaining present in
  CCA;
- acquisition uses aggregate physical power while final feasibility checks all
  required decoded output streams against sensitivity;
- strategy modules have no mutable per-reception state and emit no events;
- every new concrete NED module has a role-appropriate `@display` icon;
- the default configuration reproduces the delivered 2x1 MRC/legacy-ACK test.

### Phase 5 -- multi-stream detection

Allow valid multi-stream snapshots and implement ZF, MMSE, and
perfect-cancellation MMSE-SIC against the already preserved plan/matrix/covariance
contracts.

Acceptance:

- the canonical unnormalized ZF solution produces identity effective response
  for a full-rank 2x2 fixture, while reported output rows are unit normalized;
- ZF reports a typed undecodable result for underdetermined/rank-deficient input;
- MMSE remains finite for rank-deficient input; for a separate full-rank fixture
  it approaches ZF as noise tends to zero (no such convergence claim is made for
  rank-deficient input);
- MMSE-SIC order and ties are deterministic and output stream order is restored;
- per-stream and scalar compatibility SNIR are both observable;
- legacy preamble/header processing remains one stream.

### Phase 6 -- HT STBC representation and decoding

Add the typed STBC descriptor/plan/decoder and remove the TGn channel's STBC
rejection only after the receive-support and capability gates exist.

Acceptance:

- the STBC field combines with MCS-derived NSS exactly as Table 19-12 requires;
- the 1SS-to-2STS mapping matches Table 19-18 signs/conjugations and
  `alpha=1/sqrt(2)` normalization;
- a nonzero-subcarrier fixture applies the same Table 19-10 phase to STBC Data,
  HT-STF, and HT-LTF for each STS;
- noiseless one-Rx and two-Rx Alamouti fixtures recover both symbols exactly;
- per-slot covariance has unit trace, the augmented two-slot covariance has
  trace two, and an STBC interferer follows the augmented covariance path;
- output SINR uses both STS and all selected receive rows without double-counted
  power;
- `STBC=0` is bit-for-bit unchanged;
- unsupported layout, dimensions, or local capability fail before scalarization;
- a header-reconstructed receive-only STBC mode is accepted when locally
  supported by `htRxStbc` and the decoder, `htRxStbc=0` rejects it, and ordinary
  rate selection never enables it without negotiated peer support;
- every Table 19-18 sign, normalization, and covariance originates in the one
  shared descriptor builder and is not duplicated in transmit/decode code.

### Phase 7 -- cache/capability integration, documentation, and review

Add interference-revision and single-draw part decisions, then run one
deterministic static-matrix module configuration per strategy. Add typed HT
Capabilities exchange and per-peer MCS/width/Rx-STBC state before enabling
ordinary unicast STBC rate selection; keep group-addressed STBC disabled. Extend
the TGn integration only after pure receiver algebra passes. Update the showcase
to label the selected receiver policy and the perfect-CSI/perfect-cancellation
assumptions. Do not present packet delivery as MIMO PER validation.

## Focused verification matrix

Add or extend only directly mapped tests:

| Test | Direct claim |
|---|---|
| `tests/unit/ChannelMatrixAlgebra_1.test` | Conjugate transpose, products, HPD solves, rank/condition threshold, invalid values. |
| `tests/unit/SpatialTransmissionPlan_1.test` | Generic segment ordering, dimensions, PSD/covariance normalization, and invalid values without IEEE constants. |
| `tests/unit/ChannelMatrixReceptionMaterializer_1.test` | Eager resource-cell construction, copied inputs, boundary behavior, zero outside the domain, and query-order identity. |
| `tests/unit/ChannelMatrixCovarianceMaterializer_1.test` | Exact `A`/`Rz`, canonical interferer ordering, HPD policy, selected-row reduction, and scaling invariance. |
| `tests/unit/ChannelMatrixSnir_1.test` and `ChannelMatrixSensitivity_1.test` | Eager summaries, scalar mapper, acquisition/output-power separation, and finite failure mapping. |
| `tests/unit/RadioMediumInterferenceRevision_1.test` | Late/removed interferer, version-safe publication, retained part decisions, and Map/Vector/Reference parity. |
| `tests/unit/Ieee80211SpatialTransmissionPlan_1.test` | Direct HT mapping, Table 19-10 CSD phases, HT-STF/HT-LTF/Data boundaries, and HT-LTF count/duration. |
| `tests/unit/MimoReceiverStrategy_1.test` | Fixed/optimal antenna selection, selection combining, MRC/max-SINR, covariance, ties, zero channel. |
| `tests/unit/ChannelMatrixSpatialStreamDetection_1.test` | ZF/MMSE results, residual power, rank failure, MMSE limit, perfect-cancellation MMSE-SIC order. |
| `tests/unit/Ieee80211HtAlamoutiDecoder_1.test` | Table 19-18 mapping, one/two Rx analytical oracles, invalid NSS/NSTS/capability. |
| `tests/unit/Ieee80211HtAlamoutiRuntimeContext_1.test` | Slot-specific augmented channel/covariance and unaligned interferer timing across an Alamouti pair. |
| `tests/unit/Ieee80211PhyHeaderSerializer_1.test` | Complete six-octet HT-SIG bit positions, authoritative `8f 64 00 07 54 00` vector, CRC rejection, MCS-derived NSS, and Table 19-12 NSTS validation. |
| `tests/unit/Ieee80211PhyModeResolver_1.test` | Canonical header-derived mode with absent/matching sender metadata, deterministic rejection of mismatch, receive-support versus rate-selection membership, and `htRxStbc` gates. |
| `tests/unit/Ieee80211HtCapabilityGating_1.test` and `Ieee80211MgmtHtCapabilitiesSerializer_1.test` | Local/peer Tx/Rx-STBC, MCS/width, unknown/group peer policy, fixed-byte representation, and required Probe Request carriage. |
| `tests/unit/ChannelMatrixCombiner_1.test` | Preserve delivered selected-column MRC fixture during migration. |
| `tests/unit/TgnChannelModel_1.test` | Relaxed receiver-mode gating without changes to link/channel state. |
| `tests/unit/TgnMimoChannel_1.test` and `TgnMimoChannelStatistics_1.test` | Run only if shared matrix algebra is extracted from the TGn evaluator; channel response/distribution must not change. |
| `tests/module/Ieee80211MimoReceiver_1.test` | Static matrix, strategy selection, per-stream SNIR, interference covariance, CCA separation, HT data plus legacy header/ACK, and `listeningFilter=true` unsupported-STBC CCA. |
| `tests/module/Ieee80211HtCapabilityNegotiation_1.test` | Beacon/probe/association peer state, replacement/clear, two-receiver local-support divergence, and legal/blocked STBC rate selection. |
| `tests/module/TgnDimensionalRadioMedium_1.test` | Existing time-varying 2x1 MRC and legacy ACK compatibility. |
| `tests/module/TgnInvalidConfiguration_1.test` | Invalid plan, antenna/NSS/NSTS/STBC, covariance, strategy, and mapping diagnostics. |

Freeze these analytical fixtures:

1. For `h=[3+4j, 12j]` and `Rz=diag(1,4)`, branch SINRs are
   `[25,36]`; selection combining chooses row 1; maximum-SINR is
   `h^H Rz^-1 h = 61`.
2. For `h=[1,2]` and `Rz=[[2,1],[1,2]]`, maximum-SINR is 2 and the
   weight direction is `[0,1]`, distinguishing covariance-aware combining from
   MRC.
3. For `H=[[1,1],[0,1]]`, the canonical pre-normalization ZF matrix is
   `[[1,-1],[0,1]]`; with unit noise its projected variances are `[2,1]`.
   The reported unit-norm rows are
   `[[1/sqrt(2),-1/sqrt(2)],[0,1]]`, with projected noise powers `[1,1]`
   and desired powers `[1/2,1]`, preserving SINRs `[1/2,1]`. With unit MMSE
   regularization, the canonical pre-normalization detector is
   `[[0.4,-0.2],[0.2,0.4]]`; normalize its rows before reporting output PSD.
4. For desired `h=[1,1]`, interferer `g=[1,-1]`, and identity noise,
   maximum-SINR nulls decoded interferer power while CCA still observes it.
5. With `SelectionCombiner` downstream, four receive rows, two active chains,
   `h=[1,4,3,2]`, and identity covariance, enumerate all six size-two subsets.
   The optimum score ties for every subset containing row 1, so lexicographic
   resolution selects `[0,1]`. With three required spatial streams and only two
   active chains, ZF returns the typed underdetermined result.
6. For Table 19-18 Alamouti at `k=0` with identity direct mapping,
   `h=[1+j,2-j]`, `s0=1+2j`, and `s1=-1+0.5j`, the raw unnormalized slot
   observations are `q0=1.5+3j` and `q1=-1.5-5.5j`; the actual received values
   are `r0=alpha*q0` and `r1=alpha*q1`, where `alpha=1/sqrt(2)`. With
   `D=|h0|^2+|h1|^2=7`, decode using
   `(conj(h0)*r0+h1*conj(r1))/(alpha*D)` for `s0` and
   `(conj(h0)*r1-h1*conj(r0))/(alpha*D)` for `s1`, and require exact recovery
   within the frozen relative tolerance.
7. For HT20, two STS, identity `D`, subcarrier `k=1`, `deltaF=312.5 kHz`, and
   Table 19-10 shifts `[0,-400 ns]`, require
   `Q1=D*diag(1,exp(j*pi/4))`. The same matrix must be returned for HT-STF,
   each HT-LTF, non-STBC Data, and both slots of STBC Data.

For well-conditioned deterministic fixtures, use
`abs(actual-expected) <= 1e-12 * max(1,abs(expected))`. Test rank/condition
behavior immediately on both sides of the frozen threshold. Never use a
stochastic TGn delivery result as the primary receiver-algebra oracle.

Future debug-mode commands, run from the repository root:

```sh
make MODE=debug -j$(nproc)

MPLCONFIGDIR=/tmp inet_run_unit_tests -m debug -f \
  '(ChannelMatrixAlgebra|SpatialTransmissionPlan|Ieee80211SpatialTransmissionPlan|MimoReceiverStrategy|ChannelMatrixSpatialStreamDetection|Ieee80211HtAlamoutiDecoder|Ieee80211HtAlamoutiRuntimeContext|Ieee80211PhyHeaderSerializer|Ieee80211PhyModeResolver|Ieee80211HtCapabilityGating|Ieee80211MgmtHtCapabilitiesSerializer|ChannelMatrixCombiner)_1\.test'

MPLCONFIGDIR=/tmp inet_run_unit_tests -m debug -f \
  '(ChannelMatrixReceptionMaterializer|ChannelMatrixCovarianceMaterializer|ChannelMatrixSnir|ChannelMatrixSensitivity|RadioMediumInterferenceRevision)_1\.test'

MPLCONFIGDIR=/tmp inet_run_unit_tests -m debug -f \
  '(ChannelMatrixSnapshot|TgnChannelModel)_1\.test'

MPLCONFIGDIR=/tmp inet_run_unit_tests -m debug -f \
  '(TgnMimoChannel|TgnMimoChannelStatistics)_1\.test'

MPLCONFIGDIR=/tmp inet_run_module_tests -m debug -f \
  '(Ieee80211MimoReceiver|Ieee80211HtCapabilityNegotiation|TgnDimensionalRadioMedium|TgnInvalidConfiguration)_1\.test'
```

Run the `TgnMimoChannel|TgnMimoChannelStatistics` filter only if those tests use
changed/extracted shared matrix algebra. If shared dimensional receiver paths
change, also run only the directly mapped dimensional compatibility module tests
with an explicit filter.

From `tests/fingerprint`, preserve the existing channel-disabled dimensional
coverage:

```sh
MPLCONFIGDIR=/tmp ./fingerprinttest -d \
  -m '.*(GenericRadioWithDimensionalAnalogModel|Ieee80211RadioWithDimensionalAnalogModel).*' \
  -f 'tplx' -f '~tNl' -f '~tND' wireless-combo.csv
```

There is currently no receiver-strategy fingerprint. Do not add or update a
fingerprint baseline until the behavioral contract is stable, and never update
one without explicit user approval after explaining the changed trajectory.

## Architecture and completion gates

All currently proposed source targets are unsealed; the only recursive seal is
`src/inet/common/packet/`. Recheck the authoritative sealing status immediately
before implementation because this may change.

Map and review at least:

- `R-SCOPE-WIRELESS`, `R-SCOPE-FIDELITY`, `R-RUN-REPRO`, and
  `R-COMPOSE-NOCODE`;
- `AR-ORG-DOMAINS`, `AR-ORG-CONTRACTS`;
- `AR-MOD-COMPOSITION`, `AR-MOD-PLUGGABLE`, `AR-MOD-FIDELITY`;
- `AR-PKT-SIGNAL`, `AR-PKT-DUAL`, `AR-COM-DIRECT`;
- `AR-OBS-SIGNALS`, `AR-OBS-NED-TRUTH`, `AR-OBS-INTROSPECTION`;
- `AR-CFG-INFER`, `AR-CFG-PARAMS`;
- `AR-EXT-FEATURES`;
- `AR-QUAL-TESTS`, `AR-QUAL-DETERMINISM`, `AR-QUAL-NAMING`,
  `AR-QUAL-TRACEABILITY`, `AR-QUAL-LOGGING`, `AR-QUAL-FINGERPRINT`,
  `AR-QUAL-DISPLAY`;
- `AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`;
- `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`,
  `AR-WLAN-ARCH-VARIANTS`;
- `AR-WLAN-FRAME-REPRESENTATION`, `AR-WLAN-PHY-AUTHORITY`,
  `AR-WLAN-PHY-TIMING`;
- `AR-WLAN-OBS-EVENTS`, `AR-WLAN-QUAL-TESTS`.

After the production diff stabilizes, run focused architecture checks over the
changed common and IEEE 802.11 subtrees, then emit every item in the complete
general and WLAN semantic review checklists. Review must explicitly answer:

- Does common code depend only on generic spatial contracts and never IEEE
  modes/headers?
- Does the transmitter own one immutable mapping/power plan and does the
  receiver own all processing policy?
- Are CCA, aggregate signal power, per-stream signal power, covariance, and
  post-detection SINR distinct?
- Does every published spatial SNIR contain only eager immutable samples and
  summaries, with no numeric query dereferencing a reception, processor module,
  exact evaluator, or mutable cache?
- Can a later overlapping interferer advance one reception's revision without
  mutating its completed part decisions, mixing cache revisions, or drawing PER
  twice?
- Are aggregate-power acquisition and all-output decoded sensitivity ordered
  explicitly, with `SignalPowerInd` independent of receiver strategy?
- Are all HT-SIG fields represented on air, are NSS/NSTS derived from MCS/STBC,
  and are the resulting layouts gated by format/capability?
- Is STBC transmission gated by local Tx-STBC plus peer-advertised Rx-STBC,
  Supported MCS Set, and channel width, with unknown/group peer state handled by
  the frozen policy?
- Are all selection/order ties deterministic and independent of pointer,
  allocation, hash-container, or query order?
- Does the default one-stream path reproduce the existing MRC trajectory?
- Does every unsupported physical case return a typed undecodable result while
  malformed configuration throws early?
- Do feature-off builds prove that generic wireless common remains independent
  of IEEE 802.11?

The receiver work is complete only when:

1. The Phase 0 policies and numerical thresholds are frozen in tests and code
   comments distinguish IEEE requirements from INET receiver policies.
2. The matrix/covariance path passes exact unit oracles before any TGn
   end-to-end result is considered.
3. Each requested strategy is selectable declaratively and has deterministic
   analytical coverage, including invalid and degenerate cases.
4. HT 1SS-to-2STS STBC matches Table 19-18 and is not automatically transmitted
   without capability support.
5. Per-stream SNIR remains available and its scalar compatibility reduction is
   explicit and tested.
6. Existing selected-column MRC, time-varying TGn, legacy ACK, no-channel
   dimensional behavior, and directly mapped fingerprints remain unchanged.
7. Eager materialization, late-interferer cache revisions, part-decision
   single-draw behavior, decoded sensitivity, and physical CCA/tag separation
   pass their focused runtime gates under every communication-cache type.
8. Ordinary STBC rate selection is enabled only after typed HT capability
   exchange and per-peer MCS/width/Rx-STBC negotiation pass; receive-only support
   does not bypass that transmit gate.
9. Focused debug-mode build/tests, feature-off build, architecture checks, and
   both semantic checklists pass or have explicit dispositions.
10. The implementation report records working directory, build mode/command and
   status, exact test filters/status, configuration/run/seed, and artifact paths.
