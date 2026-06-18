# Implement IEEE 802.11ax Uplink MU-OFDMA

## Summary

Add opt-in, trigger-based UL MU-OFDMA supporting scheduled uplink traffic, BSR collection, mixed scheduled/UORA RUs, standard OBO contention, target-RSSI power control, and Multi-STA Block Ack. Preserve existing SU and DL behavior by default.

Implement in independently testable phases, beginning with the shared HE PHY and AID correctness gaps identified in the reports.

## Key Changes

### 1. Shared HE foundations

- Allocate unique association IDs at the AP, carry them in association/reassociation responses, and retain AP/STA AID mappings in the MIB. Simplified management assigns deterministic AIDs directly.
- Replace MAC-derived HE STA IDs with real AIDs for associated STAs.
- Generalize HE MU PHY metadata with:
  - DL-MU versus UL-TB PPDU format.
  - Trigger exchange ID.
  - Per-user RU, MCS, PSDU length, and duration.
- Derive channel, transmit-power, sensitivity, and noise inputs from active radios. Retain old duplicated HE parameters only as deprecated compatibility fallbacks.
- Calculate DL PPDU airtime from the longest user transmission and UL-TB airtime from the Trigger-commanded common duration.
- Apply each user’s MCS and RU geometry during duration and reception-success calculations.

### 2. Standards-shaped MAC frames

- Add bit-accurate serialization and validation for:
  - Basic and BSRP Trigger frames with common and per-user information.
  - QoS Null frames.
  - HE A-Control Buffer Status Report fields.
  - Multi-STA Block Ack with per-AID/TID acknowledgment records.
- Support both BSRP-polled and piggybacked BSR updates.
- Represent reports per access category, including queue size, selected TID, report time, and retry demand.
- Extend frame-sequence infrastructure with a generic receive-collection step that accumulates multiple packets until a deadline without affecting existing single-response sequences.

### 3. UL coordination, policy, and scheduling

- Add an opt-in `HeUlCoordinator` submodule to `HeHcf`, owning:
  - AP-side BSR/report-age and trigger-exchange state.
  - STA-side OBO/OCW and triggered-transmission state.
  - Trigger correlation, timeout, and per-RU outcome history.
- Add a separate trigger-policy interface. Its default implementation:
  - Uses Basic Trigger when fresh backlog or retry demand is known.
  - Uses BSRP for unknown/stale STAs while trigger work exists.
  - Treats reports as stale after 100 ms.
  - Enforces a configurable 1 ms minimum trigger interval.
  - Contends through the highest selected traffic AC; discovery-only BSRP uses AC_BE.
- Add `IIeee80211HeUlScheduler` with schedule input covering AID, per-AC BSR state, selected TID, report age, retries, path loss, last service, channel/TXOP limits, and random-access feedback. Output contains scheduled assignments, 26-tone RA RUs, target RSSI, MCS, and common TB duration.
- Provide:
  - A deterministic equal-RU scheduler.
  - A backlog scheduler using a least-recently-served mandatory anchor followed by backlog/airtime fitting.
- Adapt RA capacity from estimated unscheduled demand and recent busy-undecodable, collision, idle, and successful RA-RU outcomes, bounded by configurable minimum and maximum values.

### 4. Triggered exchange behavior

- AP wins an EDCA TXOP, sends BSRP or mixed Basic Trigger, collects aligned UL-TB responses, then sends one Multi-STA Block Ack.
- A scheduled STA transmits one same-TID A-MPDU after SIFS. If that TID emptied, it sends QoS Null with a fresh BSR.
- An unscheduled backlogged STA:
  - Maintains standard OBO state using configurable defaults `OCWmin=7` and `OCWmax=31`.
  - Decrements OBO by the advertised RA-RU count.
  - Selects one 26-tone RA RU when eligible.
- Scheduled STAs never also contend for RA RUs in the same exchange.
- Failed MPDUs return to normal retry state and may be retried by a later Trigger or ordinary EDCA.
- Multi-STA BA contains every scheduled user and every decoded RA user. Missing scheduled responses receive an empty acknowledgment state; unidentified collided RA senders receive no entry.
- STA power is derived from AP sensitivity plus a configurable margin and estimated path loss, clamped to radio limits.

### 5. Radio and reception model

- Correlate different-radio UL transmissions by trigger exchange ID and common timing.
- Permit the AP radio to receive multiple concurrent HE-TB RUs while retaining normal single-reception behavior elsewhere.
- Build each UL reception using its assigned RU center frequency, bandwidth, MCS, duration, and transmit power.
- Treat non-overlapping RUs as orthogonal. Same-RU UORA transmissions enter the existing SNIR/interference model, allowing realistic failure or capture.
- Deliver all successfully decoded responses to the receive-collection step at the exchange deadline.

## Test Plan

- Preserve the current baseline of 12 passing HE/OFDMA tests.
- Add serializer round-trip, field-boundary, and malformed-frame tests for Trigger, BSR, QoS Null, generalized HE PHY metadata, and Multi-STA BA.
- Test AID uniqueness, association/reassociation propagation, disassociation cleanup, and simplified-management assignment.
- Test both UL schedulers across 20/40/80/160 MHz, including RU validity, anchor rotation, TXOP fitting, RA bounds, and collision feedback.
- Test BSR polling, piggyback updates, 100 ms expiration, empty grants, and stale-report recovery.
- Test exact SIFS start, common duration alignment, partial responses, receive collection, Multi-STA BA contents, timeout, and retry paths.
- Test deterministic OBO evolution, OCW growth/reset, scheduled-versus-RA exclusion, same-RU collision/capture, and disjoint-RU isolation.
- Re-run DL tests to verify corrected duration/MCS/AID handling and unchanged opt-out behavior.
- Add a new UL OFDMA example with scheduled-only, mixed UORA, BSR refresh, collision, power-control, and retry configurations.
- Add trend-based validation for parallel airtime, throughput gain over SU under load, bounded service fairness, BSR freshness, and expected UORA collision/backoff behavior.
- Build with `-j$(nproc)` and run focused tests with ccache disabled and both OMNeT++ and INET environments sourced.

## Assumptions and Defaults

- UL MU-OFDMA is disabled by default.
- Packet-level HE PHY fidelity is required; waveform-level synchronization errors, MU-MIMO, DCM, puncturing, and multiple TIDs per STA remain out of scope.
- Mixed Trigger frames may contain scheduled and UORA RUs.
- UORA uses 26-tone RUs, exact trigger-relative timing, PHY-based collision/capture, and configurable OCW and RA-RU limits.
- Existing DL schedulers, SU EDCA, Block Ack behavior, and non-ax configurations remain compatible.
