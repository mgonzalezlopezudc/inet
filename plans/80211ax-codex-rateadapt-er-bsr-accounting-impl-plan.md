# HE Rate Adaptation, ER SU, and BSR Accounting Plan

## Summary
Implement three HE additions as first-class packet/bit-level features: a Minstrel-like HE rate controller, bit-exact HE Extended Range SU support, and complete BSR parsing/accounting. Add three focused examples under `examples/ieee80211/`: rate adaptation, ER SU range, and BSR-driven UL scheduling.

## Key Changes
- Add a reusable HE rate adaptation interface used by both SU rate selection and HE MU schedulers.
- Implement `HeMinstrelRateControl` with per-peer HE MCS/NSS state, EWMA success probability, probing, retry feedback, throughput scoring, LDPC/DCM/ER-SU constraints, and signals for selected MCS/probe/success probability.
- Keep existing fixed-rate behavior as default unless a rate-control submodule is configured; existing `IRateControl` users remain compatible.

- Extend HE PHY mode support with exact HE ER SU preamble format:
  - Add `HE_PREAMBLE_ER_SU` and corresponding PPDU/header metadata.
  - Model duplicated HE-SIG-A / ER SU signaling at bit level in serializers.
  - Enforce ER SU constraints: SU only, eligible HE MCS/DCM combinations, NSS limits, no MU/trigger use.
  - Carry ER SU fields through mode selection, PHY header serialization, receiver interpretation, and error-model selection.

- Complete BSR handling:
  - Centralize HE BSR pack/unpack helpers for the HE Variant HT Control field.
  - Make serializer round-trip BSR fields, including invalid/control-ID handling.
  - On STA HE-TB responses, report queue size after selected MPDUs are removed from the queue.
  - On AP receive, update per-AID/TID/AC backlog from QoS Data and QoS Null BSRs.
  - After Multi-STA BA, reconcile AP-side backlog using acknowledged MPDU bytes so stale reports do not over-schedule.
  - Add per-AID/TID BSR age, bytes reported, bytes scheduled, bytes acknowledged, and stale-report signals/statistics.

## Public Interfaces
- Add an HE-specific rate adaptation contract, e.g. `IIeee80211HeRateControl`, with:
  - `selectHeMode(peer, bandwidth, ruToneSize, ppduFormat, maxNss, constraints)`
  - `reportHeTxResult(peer, mcs, nss, ruToneSize, retryCount, success, ackedBytes)`
  - `reportHeRxSnir(peer, snirDb)`
- Add NED module `HeMinstrelRateControl` under `mac/ratecontrol`.
- Add NED parameters for Minstrel interval, EWMA weight, lookaround/probe ratio, supported MCS range, max NSS, and optional SNR seeding.
- Add ER SU config knobs on HE-capable WLANs: enable ER SU, default ER SU use policy, and DCM preference.
- Keep BSR fields on `Ieee80211DataHeader`, but move bit packing/parsing into named helper functions used by tests and serializer.

## Examples
- `he_rate_adaptation`: one AP and several STAs at different distances; compare fixed MCS vs `HeMinstrelRateControl`, record selected MCS/NSS, retries, throughput, and packet loss.
- `he_er_su`: one AP and edge STA; compare normal HE SU vs ER SU with bit-exact preamble and DCM, showing extended connectivity/range at lower throughput.
- `he_bsr`: UL OFDMA topology with bursty per-AC traffic; compare stale/no BSR scheduling vs full BSR accounting, showing better RU allocation and lower wasted trigger capacity.

## Test Plan
- Unit tests for Minstrel state transitions: EWMA update, probing, fallback, recovery, HE MCS validity, NSS limits, and per-peer independence.
- Scheduler tests proving DL/UL HE allocations consume rate-controller MCS decisions instead of fixed defaults.
- Serializer tests for ER SU PHY/header bitfields and BSR HT Control pack/unpack round-trips.
- PHY tests for ER SU preamble duration, duplicated HE-SIG-A, DCM constraints, and rejection of invalid MU/trigger combinations.
- Integration/fingerprint tests for the three examples.
- Run focused HE tests with:
  `bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler|HeUlScheduler).*\\.test"`

## Assumptions
- Use Minstrel-like rate adaptation, not AARF-HE or pure SNR thresholds.
- ER SU must be bit-level exact within INET’s existing PHY/header serialization model.
- Add three focused examples rather than one combined showcase or extending only existing examples.
- Existing non-HE and fixed-rate HE simulations must remain behavior-compatible unless the new modules are explicitly enabled.
