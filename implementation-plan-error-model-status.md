# IEEE 802.11 EESM implementation handoff

## Purpose and repository state

This file records the execution status of implementation-plan-error-model.md so work can continue safely in a new Codex session.

- Worktree: /home/user/omnetpp_ws/inet-eesm
- Branch: inet-eesm
- Starting HEAD: 7b0a6de5cef3e4515dedd542d742f469ab95daaa
- Status recorded: 2026-08-23
- No commit has been created for this work.
- The worktree was renamed. Use only /home/user/omnetpp_ws/inet-eesm in future commands and links.
- Preserve every existing modification and untracked file. They are implementation work or user-owned plan material.

The revised request established five binding decisions:

1. Support BCC only.
2. Pair the published beta values with exactly 52 HT20 or 108 HT40 data carriers. Use 56/114 data-plus-pilot carriers only for transmit-power allocation.
3. Make IEEE Std 802.11 authoritative for the HT three-part timing rewrite.
4. Keep `reviewed` as the production acceptance gate. The directly user-supplied table is additionally authorized for an explicit, manifest-checked `userAuthorizedLocal` evaluation mode; it is not approved for redistribution or production.
5. For the candidate supplied on 2026-08-23, duplicate the HT20 point sets exactly for HT40; this is an explicit user assumption, not flat-bandwidth validation.

The clean-room boundary in the plan remains binding. Do not inspect or derive anything from ns-3 source, tests, patches, table files, generated data, API layouts, or repository history. The primary paper is standards/3067665.3067671.pdf, SHA-256 a428fdcaa4423f331e3bd02044345ec4305ee54056c319404a897e713cdab16e.

## Overall execution status

Milestones 1 through 3 are implemented and tested:

- source/data contract and BCC-only local beta input;
- deterministic HT timing, carrier plan, EESM, and PER-table helpers;
- dimensional SISO HT integration, occupied-subcarrier spectrum, receiver/error-model integration, and rejection behavior.

Milestones 4 and 5 have now been executed to the limit supported by the supplied evidence:

- the exact 32-curve, 412-row user table is preserved unchanged in the ignored local evaluation area and bound by explicit `userAuthorizedLocal` manifests for D20/D40/E20/E40;
- the generic dimensional medium now aligns its frequency partition to occupied HT carrier metadata;
- TGn profiles A–F, path loss, channel matrices, per-carrier reception, optional shadowing, and optional whole-profile ensemble normalization are integrated;
- focused TGn/EESM unit and module tests pass, and static SISO D20/D40/E20/E40 showcase configurations are present for worktrees containing the ignored local inputs.

The default `reviewed` production gate remains deliberately unsatisfied. Independent Table-4 accuracy validation also remains open: INET packet outcomes are Bernoulli samples from this EESM model and cannot independently validate the prediction that generated them; the paper does not publish the required decoder/link-simulation observations or exact validation points. Milestone 6 documentation now includes the local artifact-backed showcase and states this boundary explicitly.

`userAuthorizedLocal` is a generic manifest policy that always requires
`deploymentScope=localEvaluationOnly`; it is not a redistribution or production
approval. Exact ignored user/paper manifests retain `unavailable`,
`notVerified`, and `notGranted` statuses, while repository-owned synthetic
fixtures may truthfully use `notApplicable` and `granted`. `reviewed` remains
the sole production acceptance gate.

### Milestones 4–5 execution artifacts

- Local-only AWGN CSV and manifests: `showcases/wireless/eesm/local/` (intentionally ignored and not distributable), with the adjacent `.d20`, `.d40`, `.e20`, and `.e40` manifest JSON files.
- Local-only publication-derived beta CSV/JSON: `showcases/wireless/eesm/local/` (`patidar2017-bcc-beta-v1.csv` and `.json`); distributable tests use committed synthetic fixtures instead.
- TGn implementation: `src/inet/physicallayer/wireless/ieee80211/channelmodel/`, `pathloss/`, `Ieee80211TgnRadioMedium.ned`, and the generic channel-matrix contracts/analog models.
- Showcase: `showcases/wireless/eesm/` with D20/D40/E20/E40 configs; `showcases/wireless/tgnchannel/` demonstrates the TGn channel itself.
- D is configured explicitly NLOS despite the paper/TGn 10 m breakpoint ambiguity; E is LOS with calibration-only ensemble normalization. All runs are static within the PPDU with large-scale shadowing, temporal variation, vehicle, and fluorescent effects disabled.

### Post-plan MI mapping alternatives

MIESM, RBIR, and MMIB have been added as separate opt-in error-model policies
without changing the default NIST path or the existing EESM configuration:

- `Ieee80211MiesmErrorModel` and `Ieee80211RbirErrorModel` share the normalized
  symbol-constrained mutual-information mapping. They are mathematically the
  same mapping for the fixed-modulation SISO scope because RBIR normalization
  cancels through the inverse.
- `Ieee80211MmibErrorModel` uses the normalized average of exact log-MAP bit-
  channel mutual information with the Gray labelling supplied by the
  authoritative HT constellation.
- The pure mapping helper uses deterministic 32-point Gauss-Hermite quadrature,
  stable log-sum-exp evaluation, and an inverse bracketed by the minimum and
  maximum carrier SNR.
- All mappings consume linear Es/N0 and use
  `gammaEffective = beta * F^-1(mean(F(gammaCarrier / beta)))`. The resulting
  effective SNR is converted to dB only for the existing AWGN PER lookup.
- Each MI policy requires an explicit finite positive scalar `beta`; its NED
  default is `NaN` and therefore fails deliberately. `beta=1` is used only by
  synthetic numerical/integration tests as an uncalibrated reference. The
  Patidar EESM beta table is never loaded or reused.
- MI policies validate only the common AWGN portion of the selected manifest.
  EESM retains its existing beta-specific artifact validation unchanged.

The implemented scope remains BCC SISO HT MCS 0-7, HT20/HT40, 52/108 static
Data-carrier SNIR values, nonzero PSDU, and WHOLE-packet corruption. These
mappings are not normative IEEE 802.11 procedures. No mapping-specific beta
calibration, independent decoder campaign, or universal accuracy claim is
included.

The implementation is opt-in. Existing IEEE 802.11/NIST defaults were not changed.

## Implemented production behavior

### Standard-authoritative signal timing

- Added immutable Ieee80211SignalPartDurations and IIeee80211Mode::getSignalPartDurations(psduLength).
- Ieee80211ModeBase retains the old generic split for non-HT modes.
- HT mixed and greenfield timing use one named-field breakdown derived from IEEE Std 802.11-2024 Clause 19.
- HT-mixed PREAMBLE is L-STF + L-LTF + L-SIG.
- HT-mixed HEADER is HT-SIG + HT-STF + all HT-LTFs.
- HT-greenfield PREAMBLE is HT-GF-STF + HT-LTF1.
- HT-greenfield HEADER is HT-SIG + remaining HT-LTFs.
- DATA is the BCC Data field in both formats.
- The transmitter consumes only the mode-provided aggregate and verifies that the three parts sum to total duration.
- SISO Data starts at 36 us for HT-mixed and 24 us for HT-greenfield. Aggregate PPDU duration behavior is unchanged.
- HT Length 0/NDP is outside the EESM lookup domain and is rejected.

### Authoritative carrier plan and dimensional spectrum

- HT20 exposes 56 occupied carriers: 52 DATA and 4 PILOT.
- HT40 exposes 114 occupied carriers: 108 DATA and 6 PILOT.
- Exact IEEE carrier and pilot indices, 312.5 kHz spacing, DC/guard exclusion, offsets, and half-open bands live in the HT mode layer.
- All occupied data-plus-pilot carriers receive equal fractions of total instantaneous power.
- Occupied bandwidth is 17.5 MHz for HT20 and 35.625 MHz for HT40; the dimensional PSD integrates to requested total power.
- Preamble and header retain the nominal boxcar spectrum. The occupied spectrum applies only to the half-open DATA interval.
- Immutable preamble/header/data descriptors are copied into the dimensional transmission analog model and survive through reception.
- Ieee80211Transmitter.dimensionalSpectrumMode is opt-in with nominalBand and occupiedSubcarriers; the default remains nominalBand.
- The occupied path rejects incompatible gain or normalization settings rather than silently changing calibration.

### PER table, EESM, and error-model integration

- Added deterministic Ieee80211PerTable and Ieee80211Eesm helpers. They make no RNG draws.
- PER interpolation uses the plan's complementary-log-log policy and stable packet-length scaling with log1p/expm1.
- PSDU length comes from HT Length. The 32-byte curve is selected through 400 bytes inclusive and the 1458-byte curve above 400 bytes.
- EESM uses stable log-sum-exp evaluation in linear SNR units.
- Only authoritative DATA carriers enter EESM: exactly 52 for HT20 or 108 for HT40.
- Raw dimensional per-carrier SNIR is consumed directly. There is no second division by 52/108, 56/114, or transmit power.
- The ignored local beta input contains all 32 published BCC values for D20, D40, E20, and E40, with explicit dataOnly 52/108 metadata.
- Ieee80211EesmErrorModel derives from the shared Ieee80211EffectiveSnirErrorModelBase pipeline, supports packet corruption only, and returns NaN for BER/SER.
- Only SIGNAL_PART_WHOLE is accepted; split-part queries fail explicitly.
- Only dimensional SNIR is accepted. Scalar SNIR fails with an explanation of the missing occupied-band normalization contract.
- Retained occupied-spectrum metadata is checked against the authoritative HT plan.
- Reception-domain DATA uses [dataStart, dataEnd): the transition at dataStart is accepted, a transition at dataEnd is ignored, and variation strictly inside DATA is rejected beyond the fixed tolerance.
- Supported modes are SISO HT BCC MCS 0-7, 20 or 40 MHz, one transmit chain, one receive chain, no STBC, and no extension spatial streams or diversity.
- Unsupported modes, antenna counts, bandwidth/calibration mismatches, scalar/nominal spectra, missing artifacts, and missing manifest fields fail explicitly.
- Receiver admission remains separate from conditional decoding. The receiver owns one Bernoulli draw for an interior PER; exact endpoints use no draw.
- Cache identity includes bandwidth, MCS, band, preamble, and guard interval.

## Production files

Modified tracked files:

- `.gitignore`
- `src/inet/common/math/{AlgebraicOperations.h,CompoundFunctions.h,PrimitiveFunctions.h}`
- `src/inet/physicallayer/wireless/common/analogmodel/dimensional/{DimensionalMediumAnalogModel.cc,DimensionalMediumAnalogModel.h,DimensionalMediumAnalogModel.ned,DimensionalSnir.h,DimensionalTransmissionAnalogModel.cc,DimensionalTransmissionAnalogModel.h,DimensionalTransmitterAnalogModel.cc,DimensionalTransmitterAnalogModel.h}`
- `src/inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h`
- `src/inet/physicallayer/wireless/common/medium/{RadioMedium.cc,RadioMedium.h,RadioMedium.ned}`
- `src/inet/physicallayer/wireless/ieee80211/mode/{IIeee80211Mode.h,Ieee80211HtMode.cc,Ieee80211HtMode.h,Ieee80211ModeBase.h}`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/{Ieee80211Transmitter.cc,Ieee80211Transmitter.h,Ieee80211Transmitter.ned}`

New production files:

- `src/inet/physicallayer/wireless/common/analogmodel/common/{ChannelMatrixResponse.cc,ChannelMatrixResponse.h,ChannelMatrixSnapshot.cc,ChannelMatrixSnapshot.h,TransmissionSpectrum.cc,TransmissionSpectrum.h}`
- `src/inet/physicallayer/wireless/common/analogmodel/dimensional/{ChannelMatrixCombiner.cc,ChannelMatrixCombiner.h,ChannelMatrixNoise.cc,ChannelMatrixNoise.h,ChannelMatrixReceptionAnalogModel.cc,ChannelMatrixReceptionAnalogModel.h,ChannelMatrixSnir.cc,ChannelMatrixSnir.h}`
- `src/inet/physicallayer/wireless/common/contract/packetlevel/{ComplexMatrix.cc,ComplexMatrix.h,IChannelMatrixSnapshot.h,IDimensionalSnir.h,IDimensionalTransmitterAnalogModel.h,IPartitionedTransmissionSpectrum.h,ITransmissionSpectrum.h,IWidebandChannelModel.h,IWidebandChannelModel.ned}`
- `src/inet/physicallayer/wireless/ieee80211/channelmodel/{TgnChannelModel.cc,TgnChannelModel.h,TgnChannelModel.ned,TgnChannelProfile.cc,TgnChannelProfile.h,TgnMimoChannel.cc,TgnMimoChannel.h}`
- `src/inet/physicallayer/wireless/ieee80211/mode/IIeee80211OfdmSubcarrierPlan.h`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211TgnRadioMedium.ned`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/{Ieee80211Eesm.cc,Ieee80211Eesm.h,Ieee80211EesmErrorModel.cc,Ieee80211EesmErrorModel.h,Ieee80211EesmErrorModel.ned,Ieee80211PerTable.cc,Ieee80211PerTable.h}`
- `src/inet/physicallayer/wireless/ieee80211/pathloss/{TgnIndoorPathLoss.cc,TgnIndoorPathLoss.h,TgnIndoorPathLoss.ned}`

No AWGN PER or publication-derived beta data is present under `src/inet/`. The plan itself, `implementation-plan-error-model.md`, is currently untracked and must be preserved.

New showcase and test material:

- `showcases/wireless/eesm/{README.md,EesmShowcase.ned,omnetpp.ini}`
- `showcases/wireless/tgnchannel/{README.md,TgnChannelShowcase.ned,generate-artifacts,omnetpp.ini,plot-artifacts.gnuplot}`
- the focused unit and module tests listed below, plus their committed synthetic fixtures under `tests/unit/data/` and `tests/module/data/`
- exact user AWGN and publication beta inputs only under ignored `showcases/wireless/eesm/local/`; the original exact user candidate copies under `tests/unit/data/` are also explicitly ignored

## Added verification

The original Milestones 1–3 unit tests remain present:

- tests/unit/Ieee80211PerTable_1.test
- tests/unit/Ieee80211Eesm_1.test
- tests/unit/Ieee80211HtPartTiming_1.test
- tests/unit/Ieee80211LegacyPartTiming_1.test
- tests/unit/Ieee80211HtSubcarrierPlan_1.test
- tests/unit/Ieee80211OccupiedSpectrum_1.test
- tests/unit/Ieee80211EesmTimeVariation_1.test

Milestones 4–5 add these focused unit tests:

- tests/unit/BilinearFunction_1.test
- tests/unit/ChannelMatrixCombiner_1.test
- tests/unit/ChannelMatrixSnapshot_1.test
- tests/unit/DimensionalCarrierGrid_1.test
- tests/unit/TgnChannelModel_1.test
- tests/unit/TgnChannelProfile_1.test
- tests/unit/TgnIndoorPathLoss_1.test
- tests/unit/TgnMimoChannel_1.test
- tests/unit/TgnMimoChannelStatistics_1.test

The original Milestones 1–3 module tests remain present:

- tests/module/Ieee80211EesmErrorModel_1.test: HT20 success and nonempty ErrorRateInd PER histogram.
- tests/module/Ieee80211EesmErrorModel_2.test: HT40/108-carrier success and nonempty ErrorRateInd PER histogram. It uses one-way UDP broadcast so a legacy 20 MHz ACK/NIST path does not contaminate the HT40 data-path check.
- tests/module/Ieee80211EesmErrorModelManifest_1.test: missing-manifest rejection.
- tests/module/Ieee80211EesmErrorModelNominal_1.test: nominal/missing occupied-spectrum rejection.
- tests/module/Ieee80211EesmErrorModelScalar_1.test: scalar-SNIR rejection.
- tests/module/Ieee80211EesmErrorModelSplit_1.test: WHOLE-only rejection.
- tests/module/Ieee80211EesmErrorModelUnavailable_1.test: default empty artifact parameters fail with the explicit unavailable-AWGN diagnostic.
- tests/module/Ieee80211EesmErrorModelUnsupported_1.test: legacy-mode rejection.
- tests/module/Ieee80211EesmErrorModelNdp_1.test: an HT Length 0/NDP PHY header reaches and proves the dedicated rejection before any SNIR or PER lookup.
- tests/module/Ieee80211EesmReceiverGates_1.test: min-versus-mean SNIR, sensitivity boundary, and exact RNG deltas 0/1/0 for PER 0/interior/1.
- tests/module/Ieee80211EesmReceiverGatesAntenna_1.test: two-receive-antenna rejection.
- tests/module/Ieee80211EesmReceiverGatesEnergy_1.test: background-noise/energy-detection behavior.

Milestones 4–5 add these focused module tests:

- `tests/module/Ieee80211EesmErrorModelLocalManifest_1.test`: explicit local synthetic-manifest acceptance.
- `tests/module/Ieee80211EesmErrorModelLocalManifest_2.test`: the same manifest is rejected by the default reviewed gate.
- `tests/module/Ieee80211EesmErrorModelLocalManifest_3.test`: local mode rejects a non-local deployment scope.
- `tests/module/Ieee80211EesmTgnD20_1.test`: static SISO TGn-D/EESM integration.
- `tests/module/TgnDimensionalRadioMedium_1.test`: dimensional TGn radio-medium integration.
- `tests/module/TgnInvalidConfiguration_1.test`: invalid TGn configuration rejection.

Committed test fixtures are synthetic and distributable:

- tests/module/data/Ieee80211Eesm.synthetic.csv
- tests/module/data/Ieee80211Eesm.synthetic-beta.csv
- tests/module/data/Ieee80211Eesm.synthetic-local.manifest.json
- tests/module/data/Ieee80211Eesm.d40.manifest.json
- tests/unit/data/Ieee80211PerTable.*.csv

The exact user candidate CSV/JSON remain physically preserved but ignored. `Ieee80211PerTable_1.test` now checks the complete 32-curve domain and HT20/HT40 equality with its synthetic fixture; exact local-artifact identity is checked separately with `cmp`, SHA-256, manifest validation, and showcase execution. None of this constitutes statistical or production acceptance.

## Verification completed

Final post-correction evidence for this tree:

1. Full `make MODE=debug -j$(nproc)` builds linked `libINET_dbg.so` successfully.
2. The combined explicit Milestones 1–5 unit filter passed 16/16, including the post-review response/snapshot dimension-mismatch constructor rejection.
3. The combined explicit Milestones 1–5 module filter passed 18/18, including carrier-aligned dimensional grids, profile tables, channel statistics, path loss, invalid configurations, and TGn/EESM D20 integration.
4. The three local-manifest cases within that module filter passed: explicit synthetic local acceptance, default-reviewed rejection, and non-local-scope rejection.
5. D20, D40, E20, and E40 each ran successfully in debug Cmdenv with seed 11 and delivered the single application packet. Result files were structurally complete. These runs prove integration only, not independent decoder accuracy.
6. `Ieee80211RadioWithDimensionalAnalogModel -r 0` and `Ieee80211MacWithIeee80211DimensionalRadio -r 0` focused fingerprints passed unchanged; no fingerprint CSV was updated.
7. `git diff --check`, the exact AWGN `cmp`, artifact SHA-256 checks, `bash -n showcases/wireless/tgnchannel/generate-artifacts`, and executable-mode verification passed.
8. The repository-wide architecture checker exited nonzero only for pre-existing baseline dependency/visualizer clusters; it named no changed file.
9. The post-plan MI/shared-pipeline unit filter passed 5/5, including independent MIESM/RBIR/MMIB numerical oracles, mapping invariants, and the existing EESM/time-variation helpers.
10. The post-plan MI/EESM compatibility module filter passed 24/24. It covers all three public MI policies, HT20/52 and HT40/108 Data-carrier paths, invalid beta values, reviewed/local artifact handling, and the shared EESM receiver/error-model boundaries.
11. Independent formal review accepted the MI extension with 15/15 general semantic checks and 14/14 IEEE 802.11 checks passing. No fingerprint row causally selects a new MI typename, so no fingerprint baseline was changed.

The showcase scalar inspection found one run/configuration, seed 11, 255 scalars, 974 parameters, one statistic, 25 histograms, and no vectors in each result. INET packet outcomes are drawn from the configured EESM PER, so these results are intentionally described only as end-to-end integration evidence.

Exact final focused commands were run from `/home/user/omnetpp_ws/inet-eesm`:

~~~sh
make MODE=debug -j$(nproc)

MPLCONFIGDIR=/tmp inet_run_unit_tests -m debug -f '(BilinearFunction_1|ChannelMatrixCombiner_1|ChannelMatrixSnapshot_1|DimensionalCarrierGrid_1|Ieee80211EesmTimeVariation_1|Ieee80211Eesm_1|Ieee80211HtPartTiming_1|Ieee80211HtSubcarrierPlan_1|Ieee80211LegacyPartTiming_1|Ieee80211OccupiedSpectrum_1|Ieee80211PerTable_1|TgnChannelModel_1|TgnChannelProfile_1|TgnIndoorPathLoss_1|TgnMimoChannelStatistics_1|TgnMimoChannel_1)\.test'

MPLCONFIGDIR=/tmp inet_run_module_tests -m debug -f '(Ieee80211EesmErrorModelLocalManifest_[123]|Ieee80211EesmErrorModelManifest_1|Ieee80211EesmErrorModelNdp_1|Ieee80211EesmErrorModelNominal_1|Ieee80211EesmErrorModelScalar_1|Ieee80211EesmErrorModelSplit_1|Ieee80211EesmErrorModelUnavailable_1|Ieee80211EesmErrorModelUnsupported_1|Ieee80211EesmErrorModel_[12]|Ieee80211EesmReceiverGatesAntenna_1|Ieee80211EesmReceiverGatesEnergy_1|Ieee80211EesmReceiverGates_1|Ieee80211EesmTgnD20_1|TgnDimensionalRadioMedium_1|TgnInvalidConfiguration_1)\.test'

MPLCONFIGDIR=/tmp inet_run_unit_tests -m debug --no-concurrent -f '(Ieee80211MutualInformationMapping_1|Ieee80211Eesm_1|Ieee80211EesmTimeVariation_1|Ieee80211HtSubcarrierPlan_1|Ieee80211PerTable_1)\.test'

MPLCONFIGDIR=/tmp inet_run_module_tests -m debug --no-concurrent -f '(Ieee80211MiesmErrorModel_1|Ieee80211RbirErrorModel_1|Ieee80211MmibErrorModel_1|Ieee80211MiErrorModelInvalidBeta_[12]|Ieee80211MmibErrorModelInvalidBeta_1|Ieee80211MiErrorModelLocalManifest_1|Ieee80211MiErrorModelLocalManifestReject_1|Ieee80211EesmErrorModelLocalManifest_[123]|Ieee80211EesmErrorModelManifest_1|Ieee80211EesmErrorModelNdp_1|Ieee80211EesmErrorModelNominal_1|Ieee80211EesmErrorModelScalar_1|Ieee80211EesmErrorModelSplit_1|Ieee80211EesmErrorModelUnavailable_1|Ieee80211EesmErrorModelUnsupported_1|Ieee80211EesmErrorModel_[12]|Ieee80211EesmReceiverGatesAntenna_1|Ieee80211EesmReceiverGatesEnergy_1|Ieee80211EesmReceiverGates_1|Ieee80211EesmTgnD20_1)\.test'

cd showcases/wireless/eesm
for config in D20 D40 E20 E40; do
    ../../../bin/inet --debug -u Cmdenv -f omnetpp.ini -c "$config" --cmdenv-express-mode=true --cmdenv-log-level=OFF
done
~~~

Exact fingerprint commands:

~~~sh
cd /home/user/omnetpp_ws/inet-eesm/tests/fingerprint
./fingerprinttest -d -m 'Ieee80211RadioWithDimensionalAnalogModel -r 0' -f 'tplx' -f '~tNl' -f '~tND'
./fingerprinttest -d -m 'Ieee80211MacWithIeee80211DimensionalRadio -r 0' -f 'tplx' -f '~tNl' -f '~tND'
~~~

Non-failing environment warnings observed:

- optional Python module py4j is absent;
- Matplotlib used a temporary cache because the normal config directory is not writable;
- generated Makefiles emitted peer-target .dll.a/.def warnings on Linux.

None affected results.

## Artifact integrity

Verified SHA-256 values:

~~~text
ecf940a8e6439975de39a33cf60ee56447d473311e13b162e30962035fc8707d  showcases/wireless/eesm/local/patidar2017-bcc-beta-v1.csv
254026038873077b5144f4a23ec380e3b62992f00e65a2e90798a4e719efd9b2  tests/module/data/Ieee80211Eesm.synthetic.csv
441389621258f397f651fd2e4247e21ac996eaec167efb99a7cbeb1f86492cf8  tests/module/data/Ieee80211Eesm.synthetic-beta.csv
364c87c3e129876f7782b312fc7a7c5b69b23c96e542ecadf113be23129cd608  showcases/wireless/eesm/local/user-supplied-bcc-awgn-per-v1.csv (32 curves, 412 data rows)
~~~

The ignored local beta JSON records BCC, all four calibration identities, publication checksum, explicit dataOnly 52/108 pairing, and 56/114 power allocation. Redistribution/production approval remains pending by design; the local manifests authorize evaluation only. Committed tests use `tests/module/data/Ieee80211Eesm.synthetic-beta.csv` and `tests/module/data/Ieee80211Eesm.synthetic-local.manifest.json`.

The HT40 synthetic manifest references the same checksums and labels its data test-only. The HT20 fixture contains corresponding embedded synthetic metadata. Loader tests exercise checksum, schema, coding, key completeness, lengths, monotonicity, and metadata failures.

## Final review findings and optional gaps

The final independent review identified one channel-matrix dimension-contract defect. `ChannelMatrixResponse` now exposes its declared dimensions, `ChannelMatrixSnapshot` rejects a mismatched response during construction, and the focused rejection test passes within the final 16/16 unit filter. No production-readiness or independent Table-4 accuracy claim is made.

Final stable-tree verdict: **ACCEPT Milestones 4–5 for the implemented local-evaluation and integration scope.** No open correctness, regression, architecture, or compliance finding remains. The complete general semantic checklist reported 15 PASS, 0 FLAG, 0 QUESTION; the WLAN checklist reported 14 PASS, 0 FLAG, 0 QUESTION. Production/redistribution promotion and decoder-accuracy claims remain blocked pending artifact approvals and independent evidence.

Important fixture history:

- Initial HT40 ping failed because its ACK used an unrelated legacy 20 MHz/NIST path. One-way UDP broadcast now isolates HT40 EESM data delivery.
- Rejection fixtures initially inherited a 22 MHz receiver bandwidth and failed before intended checks. They now set 40 MHz and reach the target paths.
- The scalar fixture now reaches dimensional-SNIR rejection rather than partial-interference rejection.
- The missing-manifest fixture now includes the dimensional radio medium and reaches the intended diagnostic.

One narrow coverage addition remains optional, not a known bug:

1. A module-level exact numeric oracle for frequency-selective EESM/no-second-normalization. Unit tests already cover flat identity, 52/108 counts, frequency vectors, and no extra normalization; module tests cover HT20 and HT40 delivery.

The independent review required direct HT Length 0/NDP coverage, so Ieee80211EesmErrorModelNdp_1.test now closes that binding matrix item. Do not broaden production behavior merely to close the remaining optional numeric-oracle gap unless a future review requires it.

## Deliberately deferred work

- No AWGN artifact accepted by the default `reviewed` production gate. The exact user-supplied CSV and checked runtime manifests are authorized only for local evaluation.
- No claim that synthetic fixtures are calibrated link results.
- No 32-curve statistical acceptance, raw-count/Jeffreys audit, confidence audit, midpoint comparison, or production flat HT20/HT40 holdout; exact 0/1 candidate endpoints cannot be converted to the plan's Jeffreys values without raw counts.
- No independent TGn D/E decoder campaign or Table-4 accuracy claim; the required non-circular outcomes/points are unavailable.
- No scalar-SNIR support.
- No MIMO, NSS greater than one, diversity, LDPC, STBC, extension streams, split reception, bit/symbol corruption, or DATA time variation.
- No interference/collision fidelity claim.
- No placeholder data is used; the locally runnable showcase uses the exact user-supplied table and identifies its limitations.
- No beta redistribution/deployment approval claim.

When independently reviewable AWGN artifacts arrive, follow the complete production gate in implementation-plan-error-model.md. Require observations CSV, runtime CSV, manifest, raw counts, provenance, provider identity, checksums, license review, clean-room confirmation, confidence information, flat-bandwidth holdouts, and interpolation validation before declaring production readiness. Do not broaden the current local authorization.

## Safe continuation sequence

1. Start in /home/user/omnetpp_ws/inet-eesm. Do not use the former path.
2. Read this handoff and implementation-plan-error-model.md before changing code.
3. Run git status --short and preserve all tracked and untracked work. Do not reset, clean, checkout, or overwrite it.
4. Inspect the complete diff, including untracked files. git diff --stat alone omits new files.
5. Read and follow the repository orchestration and architecture skills for nontrivial continuation work.
6. Rerun architecture enforcement and semantic review if required by active repository instructions. Reconcile findings against the baseline.
7. If code changes, rerun the directly related explicit debug unit/module filters recorded here, the two focused compatibility fingerprints when their contracts may be affected, diff/whitespace checks, and artifact checksums. Do not broaden to an unfiltered suite.
8. Never update fingerprint expectations without explicit user approval and first-divergence evidence.
9. Do not accept or deploy the local AWGN artifact in production until the reviewed gate passes; retain `userAuthorizedLocal` and `localEvaluationOnly` boundaries.
10. Before committing, ask whether the user wants the untracked plan and this handoff included. No commit was requested or created.

Useful final checks:

~~~sh
cd /home/user/omnetpp_ws/inet-eesm
git status --short
git diff --check
git status --short tests/fingerprint
sha256sum showcases/wireless/eesm/local/patidar2017-bcc-beta-v1.csv
sha256sum tests/module/data/Ieee80211Eesm.synthetic.csv
~~~

If the repository-local architecture skill is active, the plan names:

~~~sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh
~~~

Run it repository-wide. Do not pass the IEEE 802.11 subtree as its domain argument because that changes the dependency root and creates false internal-layer violations.
