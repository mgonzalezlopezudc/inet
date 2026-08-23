Historical `master` confirms a reusable, self-contained unit-test pattern, but not safe cherry-pickable production/test commits. The four commits mix TGax/HE APIs and changed contracts:

- `30220469cc`: adds SISO/profile/snapshot/path-loss/model tests.
- `8374e0af6f`: adds `ChannelMatrixCombiner_1.test` and `TgaxMimoChannel_1.test`.
- `14f4375cf3`: adds matrix/model/medium integration assertions.
- `ab574383b9`: completes matrix, Doppler, dimensional-medium, and HE/RBIR coverage.

All tests are inline `.test` files under `tests/unit`; no separate test-library source is needed.

### Direct port map

| Candidate file (rename `Tgax` → `Tgn`) | Reusable behavior/oracle |
|---|---|
| `tests/unit/TgaxChannelProfile_1.test` | A–F component counts, RMS delay, bandwidth expansion, normalized-power sum/common scaling, duplicate-delay preservation, invalid model/bandwidth. Copy only after replacing literals with approved TGn A–F reference data. Spatial cluster angles/spreads are TGax-specific unless TGn data provides them. |
| `tests/unit/TgaxSisoChannel_1.test` | Tap phase sign, frequency response, power gain, coherent duplicate delays, empty/negative/nonfinite validation. Generic and directly portable if SISO taps remain. |
| `tests/unit/TgaxMimoChannel_1.test` | 1×1/2×2 dimensions, overlapping-delay independence, exact-seed reproducibility, transpose/reciprocity, empirical spatial covariance, mean unit power, phase sign, invalid dimensions/profile. Keep statistical loops deterministic via explicit seeds; split if runtime is excessive. |
| `tests/unit/ChannelMatrixSnapshot_1.test` | Immutable matrix dimensions, transpose without conjugation, response lookup, rectangular matrices, finite coefficients, index/time/frequency/null validation. Generic. |
| `tests/unit/ChannelMatrixCombiner_1.test` | Exact MRC gains (`2×1 {3+4i,12i}` → 169), precoder behavior, selected columns, static time/frequency-domain combiner, L-MMSE SINR and invalid metadata/power. Port only if TGn integration includes these combiners. |
| `tests/unit/TgaxChannelModel_1.test` | Link-cache lifetime, reciprocal versus directional links, order-independent seeded realizations, stable matrix dimensions, reverse-link transpose, frequency/time grid boundaries, Doppler limit/correlation, carrier-band validation. Adapt to actual TGn parameters/API. |
| `tests/unit/TgaxDimensionalRadioMedium_1.test` | Best integration fixture: deterministic channel subclass, inline NED/INI, dynamic radios, scalar fallback versus matrix snapshot, exact 2×1 MRC ratio 169, 1×1 ratio 4, antenna dimensions, matrix metadata validation. Retain the generic medium path; remove HE/RBIR sections. |
| `tests/unit/TgaxIndoorPathLoss_1.test`, `TgaxUmiPathLoss_1.test` | Reusable structure for path-loss continuity, breakpoint/inverse range, applicability and invalid inputs, but do not copy TGax numerical values without a TGn-approved reference. |

Historical `TgaxDimensionalRadioMedium_1.test` contains HE-only dependencies that must be stripped: `Ieee80211He*` mode/RU/header/util classes, `"ax"` mode, HE trigger/MU packets, RU metadata, RBIR error model, `muMimo`, `streamStartIndex`, and trigger IDs. Replace with a simple dimensional-radio transmission fixture while retaining direct reception/noise/matrix assertions.

Use corrected INET naming consistently: `TgnChannelProfile`, `TgnSisoChannel`, `TgnMimoChannel`, `TgnChannelModel`, `TgnDimensionalRadioMedium`, and descriptions/stdout using `Tgn`; never introduce `TGn` class/file capitalization.

### Focused debug gates

After the required debug build:

```sh
make MODE=debug -j$(nproc)
```

Run only the ported unit tests:

```sh
inet_run_unit_tests -m debug -f \
'Tgn(ChannelProfile|SisoChannel|MimoChannel|ChannelModel|ChannelSnapshot|DimensionalRadioMedium)_1\.test'
```

If generic matrix contracts/combiners are part of the implementation:

```sh
inet_run_unit_tests -m debug -f 'ChannelMatrix(Combiner|Snapshot)_1\.test'
```

If the medium fixture is classified as a module test rather than a unit test, use the explicit module filter:

```sh
inet_run_module_tests -m debug -f 'Tgn.*\.test'
```

Do not run the whole unit/module suite. Historical TGax commits added no fingerprint CSV rows; do not copy or update fingerprint baselines. Fingerprints are unsuitable for random channel-matrix distributions unless a stable accepted trajectory is deliberately specified.

### Current gap and external validation

The current branch has no TGn/MIMO implementation or dedicated tests. Existing `DimensionalMedium_1.test` and wireless module tests cover scalar path loss/interference only; they cannot verify matrix response, spatial correlation, reciprocity, Doppler, or TGn A–F values. The local standards corpus has no TGn document, so A–F delays/powers, correlation targets, Doppler/statistical tolerances, and path-loss constants require an approved external TGn reference before numeric assertions are finalized.

Pass recommendation: first gate profile/SISO/matrix value-object tests, then deterministic MIMO/model tests, then the stripped dimensional-medium integration fixture; add statistical/Doppler and external-reference validation only after deterministic contracts pass.