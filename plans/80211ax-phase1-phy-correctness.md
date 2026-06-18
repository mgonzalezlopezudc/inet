# Phase 1: IEEE 802.11ax PHY Correctness and Robustness

## Summary

Replace the current HE airtime approximations and PPDU-wide decoding mode with
one authoritative per-user PHY calculation used by scheduling, packing,
transmission, and error evaluation. Implement RU/MCS/NSS/GI/DCM-aware behavior
for DL HE-MU and UL HE-TB while preserving legacy/SU behavior.

Additionally, read live radio configuration, require real association IDs,
make MU planning transactional with SU fallback, respect occupied Block Ack
windows, and calculate backlog from genuinely packable packets.

## Public Interfaces and Data Model

- Add an immutable `Ieee80211HeUserPhyParameters` value containing RU geometry,
  MCS, NSS, DCM, coding type, GI, PSDU length, data/coded bits per symbol,
  symbol count, and duration.
- Add a shared HE PHY calculator that:
  - Validates RU/MCS/NSS/DCM/GI combinations.
  - Uses RU data-subcarrier counts rather than nominal RU bandwidth.
  - Includes service and BCC tail bits, symbol rounding, and zero packet
    extension by default.
  - Returns separate preamble, header, data, and complete PPDU durations.
- Extend HE PHY metadata with a common GI and coding selection. Preserve
  per-user RU, MCS, NSS, DCM, length, and calculated duration.
- Store resolved per-user parameters in `Ieee80211Transmission`; retain the
  PPDU-wide mode only for common preamble/header and backward-compatible
  indications.
- Extend HE receive indications so upper PHY/MAC tests can inspect the selected
  user parameters.
- Extend scheduler context with live channel number, center frequency,
  bandwidth, transmit power, receiver sensitivity/noise figure, GI, and
  puncturing state.
- Extend `RuAllocation` with NSS and DCM; Phase 1 schedulers continue selecting
  NSS=1 and DCM=false unless explicitly configured by a test or policy.
- Add an acknowledgment-state query returning occupied Block Ack sequence
  numbers for a receiver/TID.
- Introduce a read-only `HeDlMuTxPlan`; planning returns an optional result
  instead of throwing for ordinary MU ineligibility.

## Implementation Changes

### 1. Authoritative HE PHY calculation

- Support 0.8, 1.6, and 3.2 us guard intervals. Keep 3.2 us as the
  compatibility default.
- Replace all copies of `estimateHeMuUserDuration()` and scheduler
  spectral-efficiency formulas with the shared calculator.
- Calculate `N_DBPS` from RU data subcarriers, modulation, code rate, NSS, and
  DCM.
- Model packet-level DCM by halving effective data bits per symbol and applying
  an ideal 3 dB scalar-SNIR diversity gain during payload error evaluation.
- Permit DCM only for standards-supported MCS/NSS combinations; reject invalid
  combinations during planning, before queue mutation.
- Model NSS as parallel coded streams sharing the selected scalar SNIR
  abstraction. NSS changes rate, symbol count, preamble HE-LTF count, and
  antenna validation; spatial-channel and MU-MIMO effects remain out of scope.
- Keep BCC as the only Phase 1 coding implementation. Represent LDPC explicitly
  but reject it until a later PHY-completeness phase.
- Treat packet extension and puncturing as disabled, explicit zero/empty values
  rather than silently approximating them.

### 2. Per-user transmission and reception

- During encapsulation, resolve every DL-MU or UL-TB user through the calculator
  and write the resulting duration into the HE header.
- Set common PPDU duration from the common preamble/header plus the maximum user
  data-symbol duration.
- Make the transmitter use this result for transmission end time and
  analog-model part durations.
- Resolve the receiving STA's user descriptor by association ID and RU. For
  UL-TB, use the sole transmitted user descriptor.
- Evaluate common preamble/header success with the PPDU-wide mode and payload
  success with the resolved user's MCS, coding, RU bandwidth, NSS,
  DCM-adjusted SNIR, and PSDU length.
- Refactor NIST and Yans HE paths to accept the resolved modulation/FEC
  parameters directly instead of reading the global transmission mode.
- For whole-frame decisions, combine common-header success with only the
  addressed user's payload success.
- Report the resolved user parameters in the receive tag while retaining the
  existing mode indication for compatibility.
- Fail reception cleanly when no valid user descriptor exists; do not decode
  using the container's global mode as a fallback.

### 3. Live radio configuration

- Populate DL scheduling context in `HeHcf` from the active radio transmitter,
  receiver, and channel:
  - Actual channel and center frequency.
  - Active HE channel bandwidth.
  - Configured transmit power.
  - Receiver sensitivity and noise figure.
  - Selected HE guard interval.
- Remove center-frequency defaults and mode-set-first-entry inference from
  `HeDlMuTxOpFs`.
- Move receiver noise figure to radio/receiver configuration, retaining the old
  HCF parameter temporarily as a deprecated fallback.
- Represent puncturing as an empty active mask. Reject non-empty masks until
  puncturing support is implemented.

### 4. Strict STA-ID handling

- Replace production uses of MAC-derived 11-bit STA IDs with an
  optional/validated association-ID lookup.
- Define named constants for legitimate special STA-ID values such as
  broadcast/unassociated allocations.
- Require a valid AID for associated DL-MU and scheduled UL-TB users.
- If an allocation lacks an AID, remove it during planning; if fewer than two
  DL users remain, use SU fallback.
- Update PHY matching and tests to use configured MIB association IDs instead
  of hashed MAC addresses.

### 5. Transactional MU planning and SU fallback

- Split DL MU construction into:
  1. Read-only candidate discovery.
  2. Read-only scheduling and packing plan.
  3. Final validation.
  4. A single commit that dequeues, assigns sequence numbers, updates ACK state,
     and creates in-progress frames.
- Perform all expected failure checks before commit, including BA state, AID,
  TXOP limit, PSDU limit, duration, packet type, and minimum user count.
- Do not launch `HeDlMuTxOpFs` until a valid plan with at least two users exists.
- When planning fails, stage the oldest eligible packet for the ordinary HCF
  path and start an SU sequence.
- Stop requeueing all existing in-progress frames before MU planning. If
  recovery/outstanding frames exist, let ordinary HCF complete them before
  opening a new MU transmission.
- Treat post-commit validation failures as internal invariants; ordinary
  traffic conditions must never leave partially removed packets or terminate
  the simulation.

### 6. Block Ack window accounting

- Derive the transmit window from the agreement's starting sequence number and
  negotiated buffer size.
- Count distinct occupied sequence numbers from outstanding ACK state for the
  same receiver/TID.
- Limit new A-MPDU entries to:
  `min(maxAmpduMpduCount, negotiatedWindowSize - occupiedSlots)`.
- Prioritize eligible retries already inside the window before assigning
  sequence numbers to new MPDUs.
- Exclude candidates whose BA window has no free slot.
- Ensure assigned sequence numbers remain inside the current transmit window;
  otherwise defer the frame to a later TXOP.

### 7. Eligibility-specific queue metrics

- Use one shared eligibility scanner for candidate discovery and final packing.
- Select one destination/TID/AC flow per STA, based on its oldest eligible
  packet.
- Count backlog only for QoS data matching that destination, TID, AC, active BA
  agreement, ACK/retry eligibility, and available BA slots.
- Include A-MPDU delimiter and padding overhead in projected PSDU bytes.
- Set HoL size/time from the first eligible packet, not simply queue index zero.
- Recompute final packability after RU/MCS selection because duration limits
  depend on the assigned PHY parameters.

## Test Plan

- Add HE calculator tests covering every RU size, MCS boundaries, NSS, all
  three GIs, DCM, service/tail bits, and symbol-rounding boundaries.
- Verify scheduler estimates, packed user duration, header duration, and
  transmission duration are identical.
- Verify two users in one DL PPDU with different MCS values receive different
  PER at the same SNIR.
- Verify RU size, NSS, GI, and DCM alter duration and payload error results as
  specified.
- Add equivalent UL HE-TB per-user decoding tests.
- Verify common duration equals the longest user and shorter users do not
  extend the transmission.
- Verify channel changes and configured power/sensitivity/noise figure reach
  the scheduler.
- Verify missing or colliding AIDs cannot produce MU allocations.
- Verify late ineligibility produces clean SU fallback with unchanged queues,
  headers, sequence numbers, and ACK state.
- Test empty, partially occupied, full, and sequence-number-wrap Block Ack
  windows.
- Test mixed destination/TID/AC queues so scheduler backlog includes only
  packable traffic.
- Retain all existing HE serialization, RU isolation, scheduling, ADDBA, and
  sequential-ack regressions.

Validation commands must run from the repository root with ccache disabled and
both environments sourced:

```sh
export CCACHE_DISABLE=1
source /home/user/omnetpp-6.4.0/setenv -f
source setenv -q
make -j$(nproc)
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
```

## Assumptions and Compatibility

- This remains a packet-level HE model, not waveform-level conformance.
- Phase 1 covers HE-MU and HE-TB per-user correctness; detailed HE-SIG
  encoding, LDPC, packet extension, puncturing, and MU-MIMO remain later
  phases.
- DCM uses the agreed packet-level abstraction: half payload rate plus ideal
  3 dB diversity gain.
- Existing non-HE and HE-SU behavior remains unchanged except for adding the
  1.6 us GI capability.
- Default GI remains 3.2 us so existing bitrate- and duration-sensitive
  scenarios do not silently change.
