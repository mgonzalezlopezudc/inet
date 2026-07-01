# UL OFDMA Reliability and HE PHY Boundary Plan

## Summary

Implement two coordinated improvements while preserving packet-level scope:

1. Complete trigger-based UL OFDMA reliability with single-TID UL A-MPDUs, per-MPDU Multi-STA Block Ack, selective retry, standards-derived response acceptance, and deterministic integration coverage.
2. Complete the selected HE PHY boundary: HE LDPC, packet extension, and preamble puncturing. Defer STBC, ER-SU, Doppler, and midambles.

All new behavior remains disabled by default.

## UL OFDMA reliability

- Replace `HeHcf`’s single `triggeredUlOriginalPacket` state with a trigger-ID-keyed UL exchange record containing the selected TID, source queue, ordered retained MPDUs, sequence numbers, RU, scheduled/RA status, and expected response timing.
- On Basic Trigger reception, construct one single-TID A-MPDU from eligible QoS MPDUs:
  - Require an active Block Ack agreement, matching AP receiver, available BA-window slots, and the Trigger-selected TID.
  - Reuse DL A-MPDU delimiter and four-byte alignment rules; add HE-TB EOF padding so the PSDU fills the Trigger-commanded duration.
  - Limit packing by negotiated Block Ack window, configured MPDU/PSDU limits, and the common HE-TB duration calculated by `Ieee80211HePhyCalculator`.
  - Retain the selected MPDUs as in-flight until the Multi-STA Block Ack arrives; emit QoS Null with BSR only when no eligible MPDU fits.
  - Keep multi-TID UL aggregation out of scope; capability defaults remain unchanged.

- Make `HeUlMuTxOpFs::processResponses()` consume per-MPDU receive-result tags from each decoded UL A-MPDU, update recipient Block Ack state, deliver only successful MPDUs upward, and construct each record’s starting sequence number and 64-bit bitmap from actual outcomes.
  - Scheduled users with no valid response get an all-zero record.
  - Decoded RA users get a record; unknown, invalid, duplicate, or colliding senders do not.
  - Preserve the existing record wire type; its bitmap becomes authoritative rather than a one-bit response marker.

- Apply Multi-STA Block Ack selectively at the STA:
  - Remove only acknowledged MPDUs from the retained exchange.
  - Mark each unacknowledged MPDU retryable and return it to normal EDCA/next-Trigger eligibility without changing acknowledged MPDUs.
  - Treat a missing record or an all-zero bitmap as a failed exchange.
  - Reset UORA OCW only when at least one transmitted MPDU is acknowledged; otherwise apply the existing failure/backoff growth.

- Add a `HeUlReceiveCollectionStep` specialized from `ReceiveCollectionStep`.
  - Bind it to the active Trigger ID and expected scheduled/RA RU allocations.
  - Admit only HE-TB responses with a matching Trigger ID, valid associated AID/RU relationship, and valid common PHY parameters.
  - Use the standard response timeout shape already used by normal Block Ack handling: `SIFS + slotTime + responseMode.getPhyRxStartDelay()`.
  - Accept starts in the interval beginning at the expected SIFS response point and ending at that timeout; classify early/late, wrong-trigger, wrong-RU, and duplicate responses as non-responses for the exchange.
  - Keep rejected frames from creating a Multi-STA Block Ack entry.

## HE PHY boundary

- Implement HE LDPC in `Ieee80211HePhyCalculator` rather than bypassing it:
  - Add HE LDPC codeword sizing, shortening, repetition, post-FEC padding, and extra-symbol handling to the shared user/PPDU calculation.
  - Use zero BCC tail bits for LDPC and ensure scheduler estimates, PHY headers, transmitter duration, UL-TB duration, and receiver parameters consume the same result.
  - Apply the repository’s packet-level LDPC PER abstraction consistently in NIST and Yans: the existing 1.5 dB LDPC coding gain convention used for HT/VHT.
  - Add `heLdpc=false` to the MIB, advertise it through existing HE capability elements, negotiate it per peer, and select LDPC only when every recipient supports it. Default to BCC otherwise.

- Propagate common HE PHY controls through `Ieee80211HeMuCommonReq`, `Ieee80211HeMuReq`, HE-MU PHY headers, receive tags, and Trigger frames:
  - Coding type, guard interval, packet-extension duration, and puncturing mask are carried end-to-end for DL-MU and HE-TB.
  - Use `Ieee80211HeOperation.defaultPeDurationUs` as the configured PE source; support only 0, 4, 8, 12, and 16 us.
  - The AP writes the selected common settings into Basic/BSRP Trigger exchanges; STAs must use them when constructing HE-TB responses.

- Enable packet-level preamble puncturing with a 20 MHz-subchannel bitmap.
  - Add a validated HCF configuration parameter whose empty value preserves today’s unpunctured channel.
  - Support only 80 and 160 MHz operation; reject punctured primary subchannels, all-punctured channels, malformed masks, and unsupported 20/40 MHz or 80+80 configurations.
  - Extend HE capability negotiation with puncturing support and allocate punctured operation only to capable peers.
  - Filter the RU catalog and validate scheduler output, transmitter requests, and receive allocations so no RU overlaps a punctured 20 MHz subchannel.
  - Carry the mask in common PHY metadata so the medium and receiver use identical active-spectrum geometry.

## Tests and acceptance criteria

- Add focused UL unit/integration tests for:
  - Two or more MPDUs in one HE-TB A-MPDU, bitmap creation, partial acknowledgment, selective retry, BA-window exhaustion, and QoS-Null fallback.
  - Correct Multi-STA Block Ack records for scheduled success, missing scheduled responses, decoded RA responses, and invalid/unidentified responses.
  - Exact response timing acceptance, last-in-window acceptance, first-out-of-window rejection, wrong Trigger ID, wrong RU, and duplicate response rejection.
  - Same-Trigger transmissions on distinct RUs decoding together; same-RU collision producing no acknowledgment; same-RU capture acknowledging only the stronger sender.

- Extend `examples/ieee80211ax/ul_ofdma` with deterministic validation configurations for scheduled UL aggregation, partial loss/retry, UORA collision/capture, and timing-skew boundaries. Assert delivered MPDU counts, bitmap contents, retry flags, and OCW transitions.

- Add HE PHY tests covering:
  - LDPC calculator boundaries across HE MCS/RU/NSS combinations, codeword shortening/repetition/extra-symbol cases, and DL/UL duration equivalence.
  - Header/tag serialization and negotiated-capability rejection when LDPC or puncturing is unsupported.
  - LDPC PER improvement over BCC at a fixed SNIR in both NIST and Yans.
  - Every permitted packet-extension duration propagated through DL-MU and HE-TB transmission timing.
  - Valid punctured 80/160 MHz layouts, rejected invalid masks/RUs, and successful isolation of valid unpunctured RUs.

- Build with `make -j$(nproc)`, then run the focused HE/DL, UL scheduler, new UL reliability, and new LDPC/PE/puncturing tests with ccache disabled and both OMNeT++ and INET environments sourced.

## Assumptions and defaults

- This remains a packet-level model; no waveform synchronization, sample-level LDPC decoder, MU-MIMO, spatial reuse, STBC, ER-SU, Doppler, or midamble work is included.
- UL aggregation is single-TID and uses the existing 64-bit Block Ack bitmap limit.
- `heLdpc=false`, packet extension `0 us`, and an empty puncturing mask are the compatibility defaults.
- MU-BAR and MU-RTS Trigger variants are not added in this initiative; the scope is reliability of the existing Basic/BSRP UL flow.
