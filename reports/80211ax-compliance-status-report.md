# IEEE 802.11ax Current Compliance Status Report

## Executive assessment

**Assessment date:** 2026-06-19  
**Revision assessed:** `7c7ecbed065c`  
**Baseline:** `reports/80211ax-compliance-gap-review.md`

The earlier gap review is materially out of date in its highest-priority
area. The repository now has an authoritative per-user HE PHY path for
DL-MU and trigger-based UL PPDUs: RU, MCS, NSS, guard interval, and DCM feed a
shared timing calculation and the NIST/Yans payload error models. The
implementation also now has HE capability/operation elements, per-peer
negotiation and DL scheduler enforcement, transactional DL-MU planning, and
per-MPDU DL acknowledgment/retry handling.

Consequently, INET is a credible **packet-level, standards-aware 802.11ax HE
OFDMA model**, especially for BCC-coded downlink MU-OFDMA. It is not a
complete IEEE 802.11ax model and must not be described as waveform or
certification conformance. The largest open feature areas are spatial reuse,
Target Wake Time (TWT), HE MU-MIMO, HE LDPC, and completion of uplink
aggregation/acknowledgment behavior.

## Method and limits

This is a source and focused-regression audit against the claims and gaps in
the baseline report. It covers the current HE MAC, management, packet-level
PHY, and unit tests. It does not substitute for an IEEE certification test
plan, interoperability testing, or sample/waveform validation.

The audit distinguishes three statuses:

- **Implemented:** the feature affects simulation behavior and has direct
  source and/or regression evidence.
- **Partial:** a useful subset is modeled, but essential standard behavior or
  validation is absent.
- **Not implemented:** no meaningful HE implementation was found in the
  reviewed paths.

## Status relative to the baseline review

| Baseline area | Current status | Current assessment |
|---|---|---|
| HE modes, RU geometry, and DL MU-OFDMA | Implemented | Standard RU layouts, 20/40/80/160 MHz operation, MCS 0--11, per-user A-MPDU packing, common PPDU timing, RU-aware medium behavior, and DL recovery are present. |
| Per-user HE PHY processing | Implemented, BCC scope | The former PPDU-wide-payload-mode gap is closed for RU/MCS/NSS/GI/DCM. HE LDPC remains explicitly rejected. |
| HE capability/operation negotiation | Implemented core, partial breadth | Capabilities and operation elements are advertised, stored, intersected, and used by DL admission/allocation. The modeled capability set is narrower than the complete HE capability space. |
| BSS coloring and spatial reuse | Not implemented | BSS color is represented and serialized, but does not drive NAV, OBSS/PD, power restriction, collision, or color-change behavior. |
| Trigger-based uplink OFDMA | Partial | Basic and BSRP triggers, scheduled/RU-random access, buffer status, target RSSI, HE-TB metadata, and Multi-STA Block Ack exist. UL A-MPDU and per-MPDU acknowledgment/retry semantics remain incomplete. |
| Target Wake Time | Not implemented | No TWT setup, wake scheduling, doze state, or scheduler integration was found. |
| MU-MIMO and spatial multiplexing | Not implemented for HE | NSS changes the packet-level rate/timing abstraction, but users do not share an RU through spatial separation; HE sounding/beamforming/precoding are absent. |
| Secondary HE PHY features | Partial | All three standard HE guard intervals and packet-level DCM are active. LDPC, STBC, packet extension, puncturing, Doppler/midambles, and ER-SU are not completed. |
| Robustness items in the baseline | Implemented | Live-radio scheduling input, association-ID validation, transactional SU fallback, occupied BA-window accounting, and eligibility-specific backlog accounting are now present. |

## Completed high-priority work

### Per-user PHY semantics and timing

`Ieee80211HePhyCalculator.h` is the authoritative calculation path. It
validates RU/MCS/NSS/DCM/GI inputs; computes data subcarriers, BCC service and
tail bits, symbol rounding, HE-LTF/preamble contribution, common data-symbol
duration, and the 5.484 ms PPDU limit. It supports 0.8, 1.6, and 3.2 us guard
intervals.

The resolved parameters are retained by `Ieee80211Transmission` and used by
the HE-specific NIST and Yans payload error APIs. This removes the baseline
failure mode in which users could carry different MCS/NSS/DCM metadata but be
evaluated using one PPDU-wide payload mode. DCM has a defined packet-level
effect: reduced payload bits per symbol plus the selected diversity abstraction.
NSS changes rate, symbol count, and LTF timing, but is not a spatial-channel
model.

Direct evidence includes:

- `tests/unit/Ieee80211HeUserPhyParameters_1.test`
- `tests/unit/Ieee80211HePerUserTransmission_1.test`
- `tests/unit/Ieee80211HePerUserErrorModel_1.test`

### HE management negotiation and DL enforcement

`Ieee80211HeCapabilities.h` models channel-width, MCS/NSS, OFDMA, DCM,
LDPC-advertisement, aggregation, MU-BAR/HE-TB Block Ack, MPDU/Block Ack, and
RU-tone capabilities. `Ieee80211Mib` retains advertised and negotiated
per-peer state. Management STA/AP paths advertise and consume HE capabilities
and HE Operation information, while `HeHcf` and `HeDlSchedulerBase` exclude
or clamp DL allocations that violate negotiated OFDMA, width, RU, MCS, or
acknowledgment support.

This is a real behavioral improvement over configuration-only enablement.
It is nonetheless a partial model of HE management: it does not represent the
entire standard capability space, notably PPE thresholds and spatial-reuse/TWT
negotiation behavior.

### DL MU-OFDMA reliability and robustness

The DL path now uses an immutable plan before queue mutation, rejects invalid
allocations early, and falls back to ordinary SU operation when fewer than two
valid MU users remain. It tracks occupied Block Ack sequence numbers, packs
only eligible packets, models A-MPDU delimiters, preserves per-MPDU receive
outcomes, and performs partial acknowledgment/selective retry. The default
acknowledgment method is MU-BAR Trigger with HE-TB Block Ack; sequential BAR
remains an explicit compatibility option.

The scheduler context obtains active channel bandwidth, center frequency,
transmit power, sensitivity, and HE guard interval from the radio/MAC
environment. Associated DL and scheduled UL users are resolved through
association IDs rather than a MAC-derived fallback.

### HE signaling representation

There is now HE-SIG-B RU allocation validation/codec support and a validated
HE-MU header serializer. This is sufficient for the model's packet-level
metadata exchange and catches invalid RU allocations, MCS, NSS, DCM, and
STA-ID values.

It remains a **standards-shaped internal representation**, not an exact
over-the-air HE-SIG implementation. For example, the HE-MU serializer writes
model fields directly and the RU codec uses an internal allocation catalog;
it should not be used to claim bit-level HE-SIG conformance.

## Remaining compliance gaps

### 1. Complete trigger-based UL OFDMA

Uplink OFDMA is functional but incomplete. `HeUlMuTxOpFs` collects one
triggered response per sender and sets `record->bitmap = 1` for a non-null
data response. Thus the Multi-STA Block Ack exchange does not acknowledge
individual MPDUs of an UL A-MPDU or support partial UL retry. The current
response collection window is also a simplified SIFS + common-duration + slot
model, rather than a standards-based arrival-tolerance policy.

Priority work:

1. Pack/parse per-user UL A-MPDUs and produce a bitmap from MPDU outcomes.
2. Retain and retry only unacknowledged UL MPDUs.
3. Add MU-BAR and MU-RTS Trigger variants where in scope.
4. Add end-to-end regression cases for different-RU success, same-RU
   collision/capture, Trigger-ID isolation, and arrival-skew boundaries.

### 2. Spatial reuse and BSS coloring

The PHY header and HE Operation carry BSS color, but it is passive metadata.
No HE path was found for intra-/inter-BSS classification, dual NAV, OBSS/PD,
associated transmit-power limits, Spatial Reuse Parameter Sets, color
collision detection, or color change. This remains the largest omitted
dense-deployment feature set.

### 3. Target Wake Time

There is no meaningful TWT implementation. Individual/broadcast negotiation,
service periods, wake/doze radio state, sleeping-station scheduling exclusion,
and power/latency statistics all remain open.

### 4. HE MU-MIMO

There is no HE MU-MIMO scheduler or receive model. Multi-stream values are
valid per-user PHY inputs, but the implementation does not schedule multiple
users on the same RU or model sounding, CSI, precoding, beamforming, spatial
isolation, or inter-user spatial interference. Existing VHT beamforming/MU-MIMO
support is not an HE MU-MIMO implementation.

### 5. Remaining PHY completeness

The following must either be implemented with behavioral effect or remain
explicitly unsupported:

- **HE LDPC:** still rejected by the HE calculator and HE-MU serializer. The
  recent LDPC work applies to HT/VHT, not HE.
- **STBC, ER-SU, packet extension, preamble puncturing, Doppler/midambles:**
  not implemented in the HE MU/OFDMA path.
- **Detailed HE signaling:** current signaling is validated packet-level
  metadata, not a full physical-field encoding/timing model.

The explicit rejection of HE LDPC and puncturing is preferable to silently
accepting metadata-only behavior, but it limits the configuration space that
can accurately be simulated.

## Regression evidence

The following commands were run from the repository root with ccache disabled
and both OMNeT++ and INET environments sourced:

```sh
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
bin/inet_run_unit_tests -m release -f "HeUlScheduler_1\\.test"
```

Results:

- **19/19** focused HE/DL tests passed.
- **1/1** UL scheduler test passed.

The focused HE/DL suite covers HE modes, RU layout and medium isolation,
per-user timing/transmission/error evaluation, HE management-element
serialization, association-ID validation, scheduler behavior, Block Ack
windows, ADDBA admission, and DL MU acknowledgment/retry sequences. The UL
scheduler test covers scheduled and random-access RU allocation.

These results demonstrate regression consistency for the implemented slice;
they do not cover the missing UL collision/tolerance cases, overlapping-BSS
spatial reuse, TWT, HE MU-MIMO, HE LDPC, or scenario-level performance claims.

## Recommended next implementation order

1. **Finish UL OFDMA reliability:** UL A-MPDU, per-MPDU Multi-STA Block Ack,
   selective retry, response-tolerance rules, and the missing end-to-end
   interference/synchronization tests.
2. **Implement dense-network behavior:** BSS color semantics, dual NAV,
   OBSS/PD, transmit-power coupling, and overlapping-BSS scenarios.
3. **Complete the HE PHY feature boundary:** HE LDPC first, then packet
   extension/puncturing and the remaining optional features as justified by
   the packet-level abstraction.
4. **Add TWT:** negotiation, service periods, radio power states, scheduler
   integration, and energy/latency validation.
5. **Add HE MU-MIMO:** only after defining a coherent packet-level CSI,
   spatial-separation, and interference abstraction.

## Final classification

For BCC-coded HE downlink MU-OFDMA, the model has progressed from
"metadata-rich but PHY-semantically incomplete" to a behaviorally connected,
well-tested packet-level implementation. It can reasonably be described as
**standards-aware HE OFDMA simulation with substantial DL MU-OFDMA coverage**.

It should not yet be described as broad 802.11ax compliance: its dense-BSS,
power-save, spatial-multiplexing, HE-LDPC, and portions of UL aggregation and
acknowledgment behavior remain unimplemented or partial.
