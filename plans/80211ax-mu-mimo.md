# HE DL MU-MIMO with TDD

## Summary

Implement opt-in 802.11ax HE downlink full-bandwidth MU-MIMO only. An AP obtains fresh CSI through standard HE trigger-based sounding, then round-robin groups compatible backlogged STAs into one full-bandwidth HE MU PPDU. Keep existing OFDMA/SU behavior as safe fallback.

Constrain behavior to IEEE 802.11-2024 HE semantics, especially clauses 26.7, 27.3.1, 27.3.3, and 27.3.11.8. This remains a scalar packet-level model, not waveform/certification conformance.

## Key changes

- Extend HE capabilities, MIB configuration, management serialization, and peer validation with the directional MU-beamformer/beamformee and sounding-feedback fields needed for DL MU-MIMO. New capability defaults are disabled.
  - Validate AP beamformer and STA beamformee roles, NSS/NSTS limits, sounding dimensions, bandwidth-specific beamformee STS, and supported feedback mode.
  - Keep current OFDMA negotiation unchanged; add a separate directional DL-MU-MIMO eligibility helper so MU-MIMO does not accidentally require OFDMA support.

- Add a `HeMuMimoCsiManager` owned by the AP HE coordination function.
  - Store CSI by associated STA and channel bandwidth with acquisition time and validity expiry.
  - Initiate sounding on demand when at least two MU-capable backlogged STAs lack fresh CSI.
  - Snapshot configured pairwise leakage into CSI on successful feedback. Configure a default plus optional per-AID-pair overrides; leakage is residual interference relative to desired signal.
  - Exclude stale, invalid, unsupported, or failed-feedback STAs from MU-MIMO while retaining their OFDMA/SU eligibility.

- Add a dedicated HE TB sounding frame sequence:
  - Broadcast HE NDP Announcement with unique AIDs and supported full-bandwidth MU-feedback parameters.
  - HE sounding NDP after SIFS, then BFRP Trigger after SIFS.
  - Simultaneous HE-TB compressed beamforming/CQI feedback in assigned trigger RUs.
  - Accept only feedback matching the sounding dialog, AID, bandwidth, and requested dimensions; missing/invalid feedback does not refresh CSI.
  - Model full-bandwidth, unpunctured, unsegmented feedback only. Partial-bandwidth sounding, feedback segmentation, and UL MU-MIMO remain out of scope.

- Generalize the DL scheduler and HE MU container from “one user per RU” to a MU-MIMO group abstraction.
  - V1 uses one full-channel RU per MU PPDU: 242/484/996/1992 tones for 20/40/80/160 MHz. No mixed OFDMA+MU-MIMO or partial-RU MU-MIMO.
  - Rotate the round-robin anchor among eligible STAs; add compatible fresh-CSI users in stable round-robin order.
  - Allocate one stream per selected user, then distribute remaining supported streams round-robin up to four per user and eight total, bounded by AP antennas/capabilities and each STA’s negotiated Rx NSS.
  - Use equal power per allocated spatial stream. Preserve the existing MCS selection and clamp it to negotiated NSS/MCS support.

- Make MU-MIMO PPDU metadata and PHY validation standards-shaped.
  - Represent group user ordering, contiguous stream start indices, total NSTS, per-user NSS, and the Table 27-31 spatial-configuration encoding.
  - Require 2–8 unique users sharing exactly one full-bandwidth RU; reject illegal spatial configurations, total NSS above eight/AP antennas, per-user NSS above four, noncontiguous stream mappings, or inconsistent duration/PSDU metadata.
  - Update HE-SIG-B/header serialization and receiver extraction to distinguish MU-MIMO user fields from existing non-MU allocations; keep the legacy OFDMA encoding behavior intact.

- Add scalar packet-level spatial behavior.
  - Scale desired reception power by each user’s spatial-stream share.
  - Derive each user SINR from desired signal, noise, and the sounded snapshot’s leakage from all co-scheduled users; feed that SINR through the existing per-user HE error model.
  - Enforce total allocated NSTS against transmitter antenna count, rather than only checking each user independently.
  - Reuse existing MU-BAR/HE-TB Block Ack and selective retry handling for MU-MIMO payloads.

## TDD and tests

Write each test before its implementation, make it pass minimally, then refactor while keeping the focused HE suite green.

- Capability and signaling tests: directional capability gating, management round-trip, legal/illegal Table 27-31 mappings, duplicate AID rejection, full-bandwidth-only validation, and serializer round-trip.
- CSI/sounding tests: exact NDP Announcement → NDP → BFRP → feedback order and SIFS timing; valid update; missing, stale, mismatched, and unsupported feedback rejection.
- Scheduler tests: round-robin rotation, fresh-CSI requirement, group stream apportionment, antenna/NSS limits, pairwise-leakage compatibility, and OFDMA/SU fallback.
- PHY/receiver tests: same-RU extraction for two or more users, desired-power split, residual-interference SINR math, independent success/failure, and invalid total-NSTS rejection.
- TXOP integration tests: successful MU payload + MU-BAR Block Ack, partial acknowledgement/retry, failed sounding fallback, disabled-mode regression, and 20/40/80/160 MHz full-bandwidth scenarios.
- Run from repository root with the OMNeT++ and INET environments sourced and `CCACHE_DISABLE=1`:
  - `bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"`
  - New MU-MIMO-focused filters during development, then the complete focused HE suite as the acceptance gate.

## Assumptions

- Scope is HE DL MU-MIMO only; UL MU-MIMO, VHT MU-MIMO, partial-RU MU-MIMO, mixed OFDMA+MU-MIMO, LDPC, puncturing, and waveform channel matrices are deferred.
- CSI comes only from successful standard-modelled trigger-based sounding; pairwise leakage is scenario-configured and deterministic.
- MU-MIMO is opt-in and defaults off. Unsupported configuration, stale CSI, sounding failures, invalid PPDU plans, or fewer than two eligible users fall back without disrupting existing DL OFDMA/SU behavior.
- The current focused HE/DL regression baseline is passing: 20/20 tests.
