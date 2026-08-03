# Pre-HT, HT, and VHT completion design note

This note records the implemented scope, standards traceability, architectural
ownership, validation evidence, and deliberately unsupported subsets for
`preht-ht-vht-completion-plan.md`.

## Seal status

The seal list was checked before source changes. All changed production paths
are below `src/inet/linklayer/ieee80211/` or
`src/inet/physicallayer/wireless/ieee80211/`; none matches the only applicable
recursive seal, `src/inet/common/packet/`. New tests, documentation, and this
plan note are outside the `src/inet` sealing policy. No seal or exception-ledger
entry was changed.

## Phase 0: executable baseline

The retained release baseline covers legacy, HT, VHT, HE, and EHT shared-MAC
operation plus the existing focused HT/VHT and HE feature tests. Representative
logs are `/tmp/inet-phase0-baseline-focused-release-unit-tests.log` and
`/tmp/inet-phase0-baseline-shared-mac-modes-module-release.log`. No fingerprint
baseline was changed.

## Phase 1: negotiated HT/VHT capability state

Typed, model-backed HT/VHT capability and operation elements are serialized in
beacon, probe, association, and reassociation management frames. Management
owns advertisements and association lifecycle; `Ieee80211Mib` owns the
directional local-TX/peer-RX and local-RX/peer-TX contracts. Detailed and
simplified management converge on that same state, and disassociation removes
it transactionally. Consumers gate width, MCS/NSS, aggregation, LDPC, sounding,
and MU roles on the negotiated contract while legacy absence remains valid.

Standards basis: IEEE Std 802.11-2024 9.4.2.54, 9.4.2.55, 9.4.2.156, and
9.4.2.157, including the HT and VHT MCS-map tables referenced by those clauses.
This satisfies `AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`, and
`AR-WLAN-ARCH-OWNERSHIP` by keeping management transport, MIB state, MAC policy,
and PHY validation separate.

Evidence includes `/tmp/phase1-final-build.log`,
`/tmp/phase1-final-htvht-focused-release.log`, and
`/tmp/phase1-final-shared-mac-modes-release.log`.

## Phase 2: shared LDPC geometry and protected TXOPs

An amendment-neutral LDPC calculator owns codeword length, shortening,
puncturing, repetition, and extra-symbol geometry. HT, VHT, and HE adapters keep
their amendment-specific PPDU geometry while deriving capacity and duration
from the shared result. BCC remains the default; HT/VHT LDPC requires mutual
capability and an explicit request.

The implemented protected-TXOP subset follows the current non-HT mixed-mode
procedure: one initial legacy RTS/CTS reservation, a single frame-sequence
owner, absolute NAV-end preservation, per-exchange TXOP admission, and defined
timeout/re-contention behavior. Obsolete L-SIG TXOP and dual-CTS alternatives
are rejected rather than represented by duplicate state machines.

Standards basis: IEEE Std 802.11-2024 19.3.11.7.5, 19.4.3, Table 19-16,
Equations (19-90) and (19-92), 9.2.5.2, 10.23.2.4, 10.23.2.8--10.23.2.11,
and 10.27.3. This preserves `AR-WLAN-PHY-AUTHORITY`,
`AR-WLAN-PHY-TIMING`, and `AR-WLAN-MAC-EXCHANGE`.

Evidence includes `/tmp/phase12-final-focused-release.log`,
`/tmp/inet-phase22-final-regression-build-release.log`,
`/tmp/inet-phase22-final-regression-unit17-release.log`, and retained traces
`/tmp/inet-phase22-final-700us-trace.log` and
`/tmp/inet-phase22-final-cts-timeout-trace.log`.

## Phase 3: VHT sounding and constrained DL MU-MIMO

`VhtHcf` is opt-in. Its sounding coordinator owns NDPA, NDP, and VHT Compressed
Beamforming exchange state and immutable CSI freshness. Fresh CSI can add a
configured packet-level receive-power benefit to later SU traffic. The AP-only
MU scheduler consumes association, negotiated roles, CSI, queue state, and PHY
limits and emits one immutable two-user plan; PHY code validates and constructs
the PPDU. Group-ID management precedes the MU exchange, and each user is served
by a serialized BAR/BA sequence.

Standards basis: IEEE Std 802.11-2024 Figures 9-75, 9-76, 9-77, and 9-87;
9.6.22.1--9.6.22.3; 9.4.1.46; Table 9-100; 21.3.10.5.2; 21.3.11.4;
21.4.3; and the VHT SIG tables 21-11, 21-12, and 21-14. The immutable scheduler
plan and independent exchange owner satisfy `AR-WLAN-MAC-MULTIUSER` and
`AR-WLAN-MAC-EXCHANGE`.

Evidence includes `/tmp/inet-phase3b-finalfinal-build.log`,
`/tmp/inet-phase3b-finalfinal-vht-he.log`,
`/tmp/inet-phase3b-finalfinal-positive42.log`,
`/tmp/inet-phase3b-finalfinal-negative3.log`, and the retained positive MU
event log `/tmp/inet-phase3b-finaltree-positive-mu.elog`.

## Phase 4: receive processing and rate selection

Receive admission applies per-member/header FCS validation and Address1
filtering before duplicate removal, Block Ack reordering, and defragmentation.
The optional security contract stages integrity/decryption before replay
detection after reordering; the default implementation is a no-op. Computed FCS
is opt-in and declared FCS remains the compatibility default.

Supported and Extended Supported Rates use typed 500-kbit/s codes with strict
serialization validation. The MIB owns distinct local operational, local BSS
basic, current-BSS basic, and peer-advertised snapshots. One immutable policy
serves both DCF and HCF. It validates configured modes, constrains unicast by
peer advertisements, applies the legacy-basic/HT-basic/VHT-basic/mandatory
group hierarchy, and distinguishes immediate Basic Block Ack from ordinary
Compressed Block Ack selection. Non-HT reference rates are derived centrally
from modulation and coding, including the HT UEQM stream-1 rule.

Standards basis: IEEE Std 802.11-2024 Figure 5-1; 9.4.2.3; 9.4.2.11;
10.6.5.4; 10.6.5.8; 10.6.6.2; 10.6.6.4; 10.6.6.5.2; 10.6.11 and Table
10-10; and 12.5.2.4.4(h). Centralized Block Ack ordering preserves
`AR-WLAN-MAC-SEQUENCE`; focused malformed-input, lifecycle, asymmetric-rate,
and exchange tests satisfy `AR-WLAN-QUAL-TESTS`.

Receive-path evidence includes `/tmp/inet-phase4-final-narrow-build.log`,
`/tmp/inet-phase4-final-narrow-receive-units.log`, and
`/tmp/inet-phase4-final-sharedmacmodes42.log`. Rate-element evidence includes
`/tmp/inet-phase4-rate-slicea-rerun-build.log`,
`/tmp/inet-phase4-rate-slicea-rerun-focused.log`, and
`/tmp/inet-phase4-rate-slicea-final-sharedmodes42.log`. Final shared-policy
evidence is `/tmp/inet-phase4-final-delta-build.log`,
`/tmp/inet-phase4-final-delta-focused.log`,
`/tmp/inet-phase4-final-sharedmodes42.log`, and the retained asymmetric trace
`/tmp/inet-phase4-final-asym-r0.elog`.

The whole-plan acceptance run passed 29 focused unit executables, all 42
SharedMacModes runs, and all three VHT DL-MU negative repetitions. Its logs are
`/tmp/inet-wholeplan-focused-units.log`,
`/tmp/inet-wholeplan-sharedmodes42.log`, and
`/tmp/inet-wholeplan-vht-dl-mu-negative.log`. The unchanged pre-existing
`Ieee80211HeMuAddbaValidation_1.test` fixture still emits packet-ownership and
undisposed-object warnings despite passing; no changed test or production path
emitted such a warning.

## Deliberate limits

This work does not claim complete 802.11n/ac support. It omits waveform
precoding, coded-bitstream LDPC decoding, STBC, VHT MU LDPC equalization, a
complete PHY SIG bit chain, obsolete L-SIG/dual-CTS protection, alternate-rate
response selection, and general VHT MU operation. The VHT sounding/MU subset is
20 MHz, AP-only, exactly two Group-ID-1 users in positions 0 and 1, one NSS per
user, and deterministic scheduling. WEP/RSN/WPA and real cryptography remain a
separate project, as do new adaptive rate-control algorithms.
