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

# Receiver-strategy follow-on implementation

Implemented on 2026-08-24 as the runtime receiver extension to the TGn channel
work above.

## Delivered behavior

The dimensional channel-matrix path now supports receiver-local, immutable
configuration of:

- all, fixed, and deterministic optimal receive-antenna selection;
- selection combining, maximum-ratio combining, and maximum-SINR/MMSE
  combining for one spatial stream;
- zero-forcing, MMSE, and perfect-cancellation MMSE-SIC spatial-stream
  detection;
- bounded HT Alamouti decoding for `NSS=1`, `NSTS=2`, and `STBC=1`, including
  per-slot channel sampling and the full augmented covariance of correlated
  STBC interferers.

`ChannelMatrixReceptionProcessor` is stateless after initialization. It
dispatches the configured strategy over immutable time/frequency resource-cell
contexts, uses minimum per-stream SINR as the scalar compatibility mapping,
and resolves optimal-subset ties lexicographically.

## Runtime integration

- Every IEEE 802.11 transmission now carries an immutable spatial transmission
  plan. Legacy PPDUs use a one-stream/one-antenna plan; canonical HT PPDUs use
  exact robust, HT-STF, HT-LTF, and data boundaries.
- `Ieee80211HtPpduLayout` is the single timing authority used by the
  transmitter, transmission, receiver, and spatial-plan builder. It validates
  mixed-format HT layout, positive data duration, integral data-symbol count,
  and the selected long/short guard interval before any plan is published.
- The HT-SIG serializer and canonical resolver cover the complete 48-bit field,
  CRC, MCS-to-NSS metadata, Table 19-12 STBC legality, LTF counts, CBW/context
  consistency, and the 40-MHz-only MCS 32 rule.
- HT transmission construction revalidates the authoritative packet HT-SIG,
  selected mode/channel, canonical description, and exact spatial-plan layout
  before publication.
- HT20, HT40, and legacy OFDM reception use technology-owned OFDM resource
  cells. DC, pilot, and guard resources are excluded from decoding; short-GI
  data uses 3.6 us symbols; STBC slots remain atomic.
- Occupied-tone PSD normalization preserves the aggregate W-valued signal
  power exposed by the existing flat 20/40 MHz dimensional model. Desired and
  interfering covariance use normalized active-tone PSD, while CCA continues
  to use eager aggregate physical power. Interferer null and pilot resources do
  not enter decoded covariance.
- Matrix SNIR construction is eager. Published SNIR, decision, and result
  objects depend only on immutable materialized values, not on later channel
  evaluations or mutable module state.
- Interference revisions retire complete derived cache generations only for an
  active matrix receiver and an actually time/range/band-overlapping newly
  added transmission. Ordinary cache expiry does not revise historical
  receptions; explicit removal can revise only unfinished receptions.
- Matrix reception is opt-in through `Ieee80211TgnRadio`. The generic
  `Ieee80211Receiver` default remains processor-free, preserving scalar and
  legacy configurations. An enabled matrix processor rejects
  `separateReceptionParts=true` because a revision-safe per-part decision
  transaction is not implemented.
- Local immutable HT capabilities gate transmit and receive MCS, spatial-stream
  count, channel width, and STBC support. Unsupported receive forms fail closed
  without hiding physical energy from CCA.
- The radio supplies capabilities through the
  `IIeee80211HtCapabilitiesConsumer` contract, so replaceable transmitter and
  receiver submodules must explicitly implement the same capability-delivery
  behavior instead of depending on concrete built-in class casts.

The configurable processor parameters are:

| Parameter | Values |
|---|---|
| `antennaSelection` | `all`, `fixed`, `optimal` |
| `activeReceiveAntennaCount` | `-1` for all, or a positive count |
| `fixedReceiveAntennaIndices` | ordered, unique zero-based indices |
| `oneStreamCombiner` | `mrc`, `selection`, `maximumSinr`, `mmse` |
| `spatialStreamDetector` | `zf`, `mmse`, `mmseSic` |

For one stream, `mmse` is the maximum-SINR linear combiner and is intentionally
an alias of `maximumSinr`.

## Focused verification

All commands below ran from
`/home/user/omnetpp_ws/inet-tgn-channels` in debug mode.

1. Debug build:

   ```sh
   make MODE=debug -j$(nproc)
   ```

   Result: PASS.

2. Receiver-strategy and runtime-value unit tests:

   ```sh
   MPLCONFIGDIR=/tmp inet_run_unit_tests -m debug -f \
     '(ChannelMatrixAlgebra_1|SpatialTransmissionPlan_1|Ieee80211PhyHeaderSerializer_1|Ieee80211PhyModeResolver_1|Ieee80211SpatialTransmissionPlan_1|MimoReceiverStrategy_1|ChannelMatrixSpatialStreamDetection_1|ChannelMatrixPhysicalPowerMaterializer_1|MaterializedSpatialReception_1|Ieee80211HtAlamoutiDecoder_1|ChannelMatrixStbcReceptionMaterializer_1|CommunicationCacheInterferenceRevision_1).*'
   ```

   Result: 12 PASS. This includes analytical ZF/MMSE/SIC and combining
   fixtures, antenna-subset selection, HT-SIG and capability rejection,
   MCS32/CBW consistency, exact HT/OFDM resources, short-GI/STBC timing,
   correlated two-slot STBC interference, eager materialization, immutable
   queries, and unity-SISO active-tone power at a 0.9 W decoded-sensitivity
   threshold. Direct runtime fixtures additionally prove that the reception
   materializer groups two Alamouti slots into one decoded block and that an
   interference revision atomically retires every derived cache object from
   the previous immutable generation.

3. TGn and receiver-strategy runtime module tests:

   ```sh
   MPLCONFIGDIR=/tmp inet_run_module_tests -m debug -f \
     '(Tgn(DimensionalRadioMedium|InvalidConfiguration)_1|Ieee80211Mimo(SelectionZf|MaximumSinrMmse|OptimalSic)Receiver_1).*'
   ```

   Result: 5 PASS. The existing positive run uses the opt-in TGn radio with the
   matrix processor and completes HT data plus its legacy ACK; the negative
   run keeps focused fail-closed configuration coverage. Three seed-37,
   static-profile-D/NLOS scenarios deliver both a one-stream frame and an
   unambiguous two-stream HT MCS 8 frame through selection+ZF,
   fixed-selection+maximum-SINR/MMSE, and optimal-selection+MRC/MMSE-SIC. The
   mode-resolver unit test independently asserts that the configured
   `14.444444 Mbps` rate selects two spatial streams. Refreshed runtime output
   confirms the effective INI selection: the 1SS frame has a 36 us preamble
   and 324 us data duration, while the 2SS frame has a 40 us preamble (two
   HT-LTFs) and 143.2 us data duration.

4. Patch whitespace validation:

   ```sh
   git diff --check
   ```

   Result: PASS.

The test runner again emitted only the optional missing-`py4j` message and
Windows DLL peer-target warnings.

## Independent review

An independent frozen-tree review concluded `PASS — READY FOR MERGE`, with no
correctness, architecture, naming, sealing, or regression blocker. It also
verified the effective 1SS/2SS PPDU timings after applying OMNeT++ first-match
INI precedence correctly. The bounded non-blocking coverage gaps are that the
STBC materializer fixture uses equal slot channels, the cache transaction test
does not exercise every `RadioMedium` invalidation trigger, and runtime
detector dispatch is established structurally by the effective 2SS PPDU rather
than by production invocation counters.

## Architecture disposition

All changed source paths were checked against the sealing ledger before edit;
none is sealed. Added common code has no IEEE 802.11 or TGn dependency, and no
changed nonvisual code adds a visualizer dependency. The architecture fitness
script reports `AR-ORG-VIS-SPLIT` clean. Its `AR-ORG-DOMAINS` check exits 1 for
a physical-layer subtree because the grep-level rule treats every
`inet/physicallayer/...` include—including a file's own header—as an upward
protocol dependency; this is the same known scope defect documented above,
not a new dependency finding. No exception-ledger or fingerprint update was
made.

## Deliberate bounds

- Runtime HT STBC transmission remains fail-closed until peer HT capability
  exchange is represented. The receive decoder and canonical inbound metadata
  path are implemented and analytically tested; the simulator will not invent
  peer Rx-STBC authority.
- Production STBC support is the requested two-slot HT Alamouti form
  (`NSS=1`, `NSTS=2`, `STBC=1`). Other legal HT STBC dimensions are recognized
  canonically but rejected by the local bounded implementation.
- The technology grid currently covers mixed-format HT and legacy OFDM. Other
  technologies retain the generic continuum materializer.
- Matrix processing requires whole-reception caching
  (`separateReceptionParts=false`).
- Existing PER/error models consume the minimum materialized stream SINR; no
  new MIMO-specific PER calibration or EESM table was introduced.
