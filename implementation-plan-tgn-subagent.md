Implementation-oriented TGn MIMO requirements report

## 1. Source authority and evidence boundary

- The authoritative channel-model specification is IEEE 802.11-03/940r4, *TGn Channel Models*, May 2004: [official Mentor document](https://mentor.ieee.org/802.11/dcn/03/11-03-0940-04-000n-tgn-channel-models.doc). It is an IEEE 802.11 Working Group contribution/evaluation model, not normative IEEE Std 802.11 behavior.
- The local processed standards corpus is fresh but contains only IEEE Std 802.11-2024 and IEEE Std 802.11be-2024; it has no 03/940 revision. Exact r4 text therefore was not corpus-verifiable.
- Normative IEEE Std 802.11-2024 Clause 19.3.12.1, corpus chunks `80211ax-2024:chunk:08164`–`08165`, establishes only the equivalent complex-baseband interface: \(y_k=H_kx_k+n\), with \(H_k\) dimension \(N_\mathrm{RX}\times N_\mathrm{TX}\). It does not prescribe TGn A–F stochastic channels.
- A readable 03/940r1 mirror and indexed r4 excerpts were used to reconstruct requirements. The official r4 file must be ingested before implementation; all details below marked “r4-unverified” are milestone-0 gates.
- IEEE 802.11-09/0308r12 is a later TGac addendum for 80/160 MHz tap expansion, not baseline TGn MVP.

## 2. Milestone 0: mandatory source lock

Before production code:

1. Acquire the exact official 03/940r4, record checksum, extract/index it, and compare r1/r2/r4, especially Appendix C and §§4.7–4.8.
2. Verify every Appendix C A–F delay/power/angle tuple below against r4.
3. Resolve from r4/reference MATLAB:
   - whole-profile power normalization;
   - correlation-matrix square-root convention;
   - component independence and temporal initialization;
   - whether “Model F third tap” means the 20 ns unique tap;
   - exact fluorescent modulation equation/tap indexing;
   - reciprocal-link semantics, which 03/940 does not appear to prescribe.
4. Obtain the reference MATLAB generator if licensing permits and use it as a statistical oracle.

## 3. Minimum faithful data model

Do not flatten a profile into unique delays. Store cluster components:

```text
Component { clusterId, excessDelay, relativePower }
Cluster   { clusterId, meanAoA, receiverAS, meanAoD, transmitterAS }
Profile   { model A..F, components[], clusters[], largeScaleParameters }
```

Components from different clusters may share a delay but have different spatial correlations. They must be realized independently and summed coherently in \(H(f,t)\); their expected linear powers, not amplitudes, add to the SISO PDP.

Reference profile summary:

| Model | RMS delay spread | Clusters | Unique delays | Breakpoint |
|---|---:|---:|---:|---:|
| A | 0 ns | 1 | 1 | 5 m |
| B | 15 ns | 2 | 9 | 5 m |
| C | 30 ns | 2 | 14 | 5 m |
| D | 50 ns | 3 | 18 | 10 m |
| E | 100 ns | 4 | 18 | 20 m |
| F | 150 ns | 6 | 18 | 30 m |

Exact per-cluster delay ns / relative-power dB data reconstructed from Appendix C:

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

These powers match the local `master` implementation citing r4. B was independently visible in an r4 excerpt; the complete C–F values remain milestone-0 r4-verification items.

Appendix C cluster tuples `(AoA°, Rx AS°, AoD°, Tx AS°)`:

```text
A: (45,40,45,40)
B: (4.3,14.4,225.1,14.4); (118.4,25.2,106.5,25.4)
C: (290.3,24.6,13.5,24.7); (332.3,22.4,56.4,22.5)
D: (158.9,27.7,332.1,27.4); (320.2,31.4,49.3,32.1);
   (276.1,37.4,275.9,36.8)
E: (163.7,35.8,105.6,36.1); (251.8,41.6,293.1,42.5);
   (80.0,37.4,61.9,38.0); (182.0,40.3,275.7,38.7)
F: (315.1,48.0,56.2,41.6); (180.4,55.0,183.7,55.2);
   (74.7,42.0,153.0,47.4); (251.5,28.6,112.5,27.2);
   (68.5,30.7,291.0,33.0); (246.2,38.2,62.3,38.0)
```

B and D match the source-faithful local implementation; B/F had indexed r4 corroboration. A/C/E and remaining exact tuple precision are r4-unverified.

## 4. Spatial/MIMO requirements

For cluster component \(c\):

\[
H_c=R_{\rm rx,c}^{1/2}H_{\rm iid,c}(R_{\rm tx,c}^{1/2})^T
\]

where \(H_{\rm iid,c}\) is \(N_\mathrm{RX}\times N_\mathrm{TX}\), with independent \(\mathcal{CN}(0,1)\) entries; real and imaginary parts each have variance \(1/2\).

Frequency response:

\[
H(f,t)=\sum_c H_c(t)e^{-j2\pi(f-f_\mathrm{ref})\tau_c}
\]

- Support arbitrary \(N_\mathrm{RX}\times N_\mathrm{TX}\), at least 1×1, 2×2, and 4×4.
- All taps in a cluster share its angular parameters.
- Cluster mean AoA and AoD are independent uniform azimuths in the stochastic formulation; Appendix C supplies fixed reference draws. Default to the fixed table for reproducibility/comparability.
- The reference model is horizontal azimuth only; no elevation spread.
- Laplacian PAS:

\[
p(\theta)=\frac{1}{\sqrt2\sigma}
e^{-\sqrt2|\theta|/\sigma}
\]

- For a ULA, \(D=2\pi d/\lambda\):

\[
\rho(D)=\int_{-\pi}^{\pi}p(\phi)
e^{jD\sin\phi}\,d\phi
\]

with real/imaginary cosine/sine integrals as in §4.2–4.4. Use actual spacing and wavelength; half-wavelength ULA may be the initial supported geometry.

Correlation invariants: Hermitian PSD, unit diagonal, \(|\rho|\le1\), and the matrix square root reconstructs the intended correlation.

## 5. LOS, path loss, shadowing, and normalization

Large-scale path loss, 03/940r4 §2/Table I:

\[
L(d)=
\begin{cases}
L_\mathrm{FS}(d),&d\le d_\mathrm{BP}\\
L_\mathrm{FS}(d_\mathrm{BP})+35\log_{10}(d/d_\mathrm{BP}),&d>d_\mathrm{BP}
\end{cases}
\]

Thus the slopes are 20 and 35 dB/decade. Shadow fading is Gaussian in dB:

```text
A: sigma 3 dB LOS / 4 dB NLOS
B: 3 / 4
C: 3 / 5
D: 3 / 5
E: 3 / 6
F: 3 / 6
```

LOS is assumed only within the breakpoint. Define the zero/reference-distance policy explicitly and cache shadowing per link; update/correlation behavior is not specified sufficiently by the available evidence.

LOS K-factor on the first tap only, §4.1/Table II:

```text
A=0 dB, B=0 dB, C=0 dB, D=3 dB, E=6 dB, F=6 dB
```

All other taps and all NLOS components have \(K=0\) linear. Add the deterministic first-tap LOS component, conventionally at 45° AoA/AoD:

\[
H_j=\sqrt{P_j}\left[
\sqrt{\frac K{K+1}}H_F+
\sqrt{\frac1{K+1}}H_V
\right]
\]

The document’s construction starts with the NLOS PDP and adds the LOS power; do not renormalize the increased first-tap/total LOS power away.

For NLOS, convert printed dB values to linear power and keep small-scale gain separate from path loss. Normalizing total expected NLOS gain to one is what local `master` does, but the exact r4/reference-program normalization convention remains a milestone-0 decision.

## 6. Time evolution and optional high-fidelity effects

Base Doppler, §4.7.1:

\[
S(f)\propto \frac1{1+9(f/f_d)^2},\qquad f_d=v/\lambda
\]

with \(v=1.2\) km/h by default and optional truncation near \(5f_d\). Its normalized autocorrelation is:

\[
\rho(\Delta t)=e^{-2\pi f_d|\Delta t|/3}
\]

Use a continuous cached process, never independent per-packet draws, and normalize synthesis weights so marginal tap power is preserved.

Defer these until the static model is validated:

- Model F moving-vehicle spike on the “third tap”: \(v=40\) km/h, \(B=0.5\), \(\alpha=0.02\), \(C=36/\alpha^2=90000\). Exact tap interpretation is r4-unverified.
- Fluorescent-light modulation for selected D/E taps, with 100 Hz European or 120 Hz US fundamental behavior, harmonics and random phases. Exact formula/indexing is r4-unverified.
- Polarization, §4.8: reference XPD appears to be 10 dB for fixed LOS and 3 dB for variable NLOS. The contribution does not provide a complete cross-polar antenna-correlation model; any full polarized implementation requires a separately documented assumption.
- Stochastic DS/AS profile generation rather than fixed Appendix C tables.
- TGac 80/160 MHz expansion from 09/0308r12.

## 7. Reproducibility requirements

03/940 specifies distributions but not INET stream ownership. INET should:

- use explicit OMNeT++ RNG streams or stable per-link substreams;
- derive link keys from stable radio identities, never pointers or container order;
- separate substreams for shadowing, profile angles, component matrices, Doppler phases/oscillators, and optional special effects;
- cache immutable/profile state per link;
- keep fixed Appendix C angles fixed across realizations;
- define reciprocity explicitly. If enabled, reverse link response should reuse the realization and return \(H_{BA}=H_{AB}^T\), with swapped dimensions. Reciprocity is an implementation decision, not a verified 03/940 requirement.

## 8. Prioritized implementation plan

MVP:

1. Milestone-0 source ingestion and table lock.
2. Add validated A–F profile value objects preserving cluster components.
3. Add static, same-polarization NLOS Kronecker generation for arbitrary matrix dimensions and a half-wavelength ULA.
4. Add immutable link channel responses and coherent frequency-domain evaluation.
5. Integrate as a pluggable radio-medium/analog-channel function, leaving MAC behavior unchanged.
6. Add deterministic per-link lifecycle, seeds, and an explicit reciprocity option.
7. Add path loss/shadowing and first-tap LOS/K as separately visible composition stages, preventing double application.

Later fidelity:

- Bell-spectrum time evolution;
- Model F vehicle and D/E fluorescent effects;
- polarization;
- stochastic profile generation;
- arbitrary arrays/elevation/coupling;
- TGac 80/160 MHz and later TGax variants.

## 9. INET integration and existing source behavior

Current checked-out pipeline remains scalar/analog: `RadioMedium::computeReception()` delegates to `IMediumAnalogModel`; `DimensionalMediumAnalogModel::computeReceptionPower()` composes propagation, scalar path loss, obstacles, and antenna gain. TGn should be a pluggable wideband/spatial channel contract in that PHY/medium boundary, not MAC logic and not overloaded into scalar `IPathLoss`.

Secondary implementation reference, not authority:

- Commit `8374e0af6f` adds `TgaxMimoChannel.{h,cc}` and `ChannelMatrixCombiner`.
- `TgaxMimoChannel::create()` implements static B/D NLOS Kronecker channels, fixed half-wavelength ULA, only one or two antennas per endpoint, SplitMix64 realization, and coherent frequency response.
- `TgaxChannelProfile.cc` on `master` contains the A–F component tables, normalizes total linear power, but only B/D spatial metadata.
- `TgaxChannelModel.cc` adds cached link seeds and a Bell-spectrum SISO approximation, while forbidding simultaneous time-varying spatial fading.
- These are useful source-faithful slices but do not meet full TGn A–F MIMO, LOS, large-scale, polarization, or time requirements.

Applicable architecture constraints: `AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`, `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`, `AR-WLAN-ARCH-VARIANTS`, `AR-WLAN-PHY-AUTHORITY`, `AR-WLAN-PHY-TIMING`, `AR-WLAN-OBS-EVENTS`, `AR-WLAN-QUAL-TESTS`, plus `AR-MOD-PLUGGABLE`, `AR-MOD-FIDELITY`, `AR-CFG-PARAMS`, `AR-QUAL-DETERMINISM`, and `AR-QUAL-TRACEABILITY`.

## 10. Focused regression invariants

- Exact component/cluster table equality and approximate RMS spreads 0/15/30/50/100/150 ns.
- Overlapping-delay components retain cluster identity and sum coherently.
- \(H\) is always \(N_\mathrm{RX}\times N_\mathrm{TX}\); 1×1 reduces to SISO.
- Sample covariance converges to the intended Kronecker covariance.
- Same seed/link/configuration is bit-reproducible; different links/substreams vary; reciprocal dimensions transpose correctly.
- Path loss is continuous at the breakpoint, with correct slopes and shadow sigma.
- LOS affects only the first tap, has the expected K ratio, and is not normalized away.
- Time-enabled marginal power and autocorrelation match the Bell model.
- Disabled TGn mode preserves legacy scalar-channel behavior.

No simulation, packet capture, or runtime comparison was run. Therefore no observed MAC/PHY divergence is claimed. The first intended behavioral divergence is at radio-medium channel transfer/power construction before receiver decoding; PHY construction and all MAC transitions should remain identical until receiver success/SNIR outcomes legitimately differ.