# TGn wideband MIMO channel implementation report

## Result

The plan is implemented in `/home/user/omnetpp_ws/inet-tgn-channels` as an
opt-in, time-varying TGn A--F wideband MIMO channel for dimensional IEEE 802.11
radios. The implementation keeps a complete Rx-row/Tx-column complex matrix,
supports 1--8 antennas per side, uses one selected transmit column with ideal
MRC, and preserves the original dimensional-medium path when no channel model
is configured.

Time variation is part of the delivered implementation. The base Bell process,
Model F vehicle spectrum, and Models D/E fluorescent modulation are created as
continuous absolute-time processes in cached link state. Channel evaluation is
immutable, consumes no RNG, and is query-order independent.

The source model is the nonnormative IEEE working-group contribution
`standards/11-03-0940-04-000n-tgn-channel-models.pdf`, SHA-256
`d324baf8f3943ea4ee841cfc3bdf35cafff488e99e979cda290e945b1ea82463`.
The matrix convention is Rx rows and Tx columns, consistent with the baseband
interface in IEEE Std 802.11-2024 Clause 19.3.12.1.

## Frozen design evidence

- Appendix C profiles A--F are stored without flattening overlapping cluster
  components. Component counts are 1/12/18/27/38/41 and whole-profile diffuse
  power is normalized to one.
- RMS delay values recomputed from the rounded tables are
  0, 15.646634945155343, 33.4393253316208, 50.16260546026875,
  98.98424350857287, and 148.80372307797091 ns. The nominal profile metadata
  remains 0/15/30/50/100/150 ns.
- Spatial integration uses 8192 composite-Simpson panels per smooth half of
  the circular Laplacian PAS. Across every A--F Rx and Tx cluster, ULA lags
  0--7, and spacings 0.25/0.5/1.0 wavelengths, its maximum difference from
  16384 panels is `2.04734562935e-11`, below the `1e-10` gate. The previous
  4096 count was rejected because its worst difference was
  `3.27632303694e-10`.
- The Bell bank default is 256 oscillators. For candidates 32/64/128/256, the
  maximum expected normalized-autocorrelation errors at zero through five
  integer coherence-time lags are respectively
  `0.044811065951`, `0.0936173197152`, `0.0406966411225`, and
  `0.0188910430485`; 256 is the first candidate below 0.03. A normalized sum of
  independent proper complex-normal coefficients has exact unit ensemble
  variance for every count, so its fixed-time marginal and capacity
  distribution are count-invariant.
- The deterministic seed contract includes a committed
  `INET-TGN-SEED-V1`/FNV-1a/SplitMix64 golden value of
  `15695006415662829421`, exact two-word master extraction, and Mersenne
  Twister integer/uniform/normal golden checks.
- Table III 4x4 NLOS means from 20 fixed batches of 2000 realizations are:

  | Model | Mean bit/s/Hz | 95% CI half-width | Percent of iid |
  |---|---:|---:|---:|
  | A | 9.02419 | 0.01262 | 82.50% |
  | B | 8.91028 | 0.01078 | 81.46% |
  | C | 8.61352 | 0.01232 | 78.75% |
  | D | 9.98151 | 0.01167 | 91.25% |
  | E | 9.24045 | 0.01011 | 84.48% |
  | F | 10.4159 | 0.00921 | 95.23% |
  | iid | 10.9382 | 0.01251 | 100% |

## Delivered source surface

- Generic contracts and immutable values: `IWidebandChannelModel`,
  `IChannelMatrixSnapshot`, `ChannelMatrixResponse`, and
  `ChannelMatrixSnapshot`.
- Generic receiver integration: selected-column combiner, matrix reception,
  matrix-aware noise/SNIR, deterministic piecewise-bilinear materialization,
  and optional `RadioMedium.channelModel` composition.
- TGn implementation: exact profiles, deterministic spatial correlation/root,
  Bell/vehicle/fluorescent processes, LOS, stable purpose-derived link state,
  optional ordinary-transpose reciprocity, and indoor breakpoint path loss.
- Declarative configuration: `Ieee80211TgnRadioMedium`, with one authoritative
  profile/condition and time variation enabled by default.
- Showcase: fixed-seed A--F, SISO, 2x1 MRC, 2x2 matrix, and static diagnostic
  configurations plus reproducible CSV/PNG generation.
- Tests: matrix/value/math unit tests, exact/statistical TGn tests,
  invalid-configuration coverage, and a time-enabled Model D 2x1 data/legacy
  ACK end-to-end module test.

`Ieee80211EesmErrorModel` was intentionally not changed. End-to-end packet
results use the existing scalar NIST policy as an integration smoke test, not
as a TGn MRC PER oracle.

## Build and verification

All commands ran from the working directory above unless a different directory
is stated.

1. Enabled debug build:

   ```sh
   make -j$(nproc) MODE=debug
   ```

   Result: PASS.

2. Common-layer feature-off build: `Ieee80211` was disabled with
   `opp_featuretool disable -f Ieee80211`, while
   `PhysicalLayerWirelessCommon` remained enabled, followed by the same debug
   build command. Result: PASS. The original feature state and NED exclusions
   were restored; their final SHA-256 values are
   `1c6875f19fb861c964e8ea1ecf4a5ada78b0291fb625df4f418d76d80eb9bc7c`
   and `e4208ceb2887dfd0c3fc929ab598bd438398a4432e2d40bac29ee39812852a4c`.

3. Focused unit suite:

   ```sh
   MPLCONFIGDIR=/tmp inet_run_unit_tests -m debug -f \
     '(BilinearFunction|DimensionalMedium|TgnChannelProfile|ChannelMatrixSnapshot|ChannelMatrixCombiner|TgnMimoChannel|TgnChannelModel|TgnIndoorPathLoss|TgnMimoChannelStatistics)_1\.test'
   ```

   Result: 9 PASS.

4. TGn module suite:

   ```sh
   MPLCONFIGDIR=/tmp inet_run_module_tests -m debug -f \
     'Tgn(DimensionalRadioMedium|InvalidConfiguration)_1\.test'
   ```

   Result: 2 PASS. The acceptance run uses seed set 11, Model D NLOS,
   `timeVariation=true`, `reciprocal=true`, 1 Tx antenna, 2 Rx antennas, HT
   data, and its ordinary legacy ACK.

5. Existing dimensional reception/interference compatibility suite:

   ```sh
   MPLCONFIGDIR=/tmp inet_run_module_tests -m debug -f \
     '(Interference|ReceptionState)_APSKDimensionalRadio_.*\.test'
   ```

   Result: 11 PASS. The bilinear integration extension keeps the original
   constant/unilinear boundary path, which prevents channel-disabled APSK
   reception-state changes.

6. Channel-disabled fingerprints, from `tests/fingerprint`:

   ```sh
   ./fingerprinttest -d \
     -m '.*(GenericRadioWithDimensionalAnalogModel|Ieee80211RadioWithDimensionalAnalogModel).*' \
     -f 'tplx' -f '~tNl' -f '~tND' wireless-combo.csv
   ```

   Result: 40 PASS; all results equal their existing expectations. No
   fingerprint baseline was changed.

7. Showcase artifacts, from `showcases/wireless/tgnchannel`:

   ```sh
   ./generate-artifacts
   ```

   Result: 2 PASS and gnuplot completed. Generated ignored artifacts are:

   - `results/tgn-frequency-response.csv` and `.png`
   - `results/tgn-time-evolution.csv` and `.png`
   - `results/tgn-capacity-cdf.csv` and `.png`

The test runner's missing optional `py4j` message and Windows DLL peer-target
warnings are unrelated environment warnings; no selected test failed.

## Architecture and semantic review

The exact planned architecture-checker commands were run for both
`src/inet/physicallayer/wireless/common` and
`src/inet/physicallayer/wireless/ieee80211`. Both report a pre-existing checker
scope defect: the `AR-ORG-DOMAINS` rule treats any include containing
`physicallayer` as an upward protocol dependency, including a file's include of
its own header inside the checked physical-layer scope. It consequently emits
hundreds of findings over unchanged source (658 output lines for common and
449 for ieee80211) and exits 1. `AR-ORG-VIS-SPLIT` passes in both invocations.

Disposition: focused checks over every changed/new file pass. All 23 changed
generic-common files contain zero TGn or IEEE 802.11 identifiers/includes, and
all 37 changed/new nonvisual source files contain zero visualizer includes.
The IEEE-disabled build independently proves that the generic implementation
does not depend on IEEE 802.11 source.

The general and IEEE 802.11 semantic checklists were reviewed with these
results:

- The generic contract is necessary for replaceable wideband channels and is
  free of IEEE-specific constants.
- `TgnChannelModel` alone owns mutable link/RNG state; `TgnMimoChannel` is a
  pure evaluator.
- Logging, tests, CSV generation, and plots observe values without changing
  channel state.
- Desired decoded power, CCA power, background noise, and unsupported
  covariance-aware MRC interference are kept distinct.
- Omitting `channelModel` executes the original dimensional reception, noise,
  and SNIR branches, confirmed by fingerprints and APSK tests.
- Physical NED parameters have units where dimensional, defaults, and a single
  purpose. New concrete modules have display icons.
- TGn is opt-in. Merely compiling it does not change legacy IEEE 802.11 modes,
  MAC state, timing, frame sequencing, or rate selection.
- 03/940r4 is identified as nonnormative model provenance; the Rx-row/Tx-column
  convention is separately traced to IEEE Std 802.11-2024.

No persistent architecture exception or fingerprint expectation was added.
