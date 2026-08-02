# Completing pre-HT, HT, and VHT support

This plan covers the highest-value remaining IEEE 802.11 functionality before
HE. It supersedes only the *unfinished* items in
[`802.11ac-completion-and-fixes.md`](802.11ac-completion-and-fixes.md): recent
work already added VHT LDPC selection, 80+80-MHz spacing, VHT aggregation
limits, and the VHT capability data types. It does not duplicate that work.

The delivery order is intentionally driven by model correctness and reuse:

1. make advertised HT/VHT capabilities real association state and enforce them;
2. complete HT/VHT LDPC geometry and protected-TXOP behavior;
3. reuse the HE sounding/CSI/scheduling design for VHT beamforming and DL
   MU-MIMO;
4. close shared receive-path and rate-selection correctness gaps.

Security (WEP/RSN/WPA), mesh MCF, and new rate-control algorithms are valuable,
but remain separate projects: they would otherwise dominate this completion
work without enabling the advertised HT/VHT PHY capabilities.

## Scope and architectural constraints

The existing MAC declares HT/VHT support partial/incomplete
([`Ieee80211Mac.ned`](../src/inet/linklayer/ieee80211/mac/Ieee80211Mac.ned)).
The plan applies these requirements throughout:

- `AR-WLAN-STD-TRACE` and `AR-WLAN-STD-GATING`: cite the applicable IEEE
  802.11 revision/clause at each new normative decision, and retain legacy
  behavior unless both mode and negotiated capabilities allow the new path.
- `AR-WLAN-ARCH-BOUNDARIES` and `AR-WLAN-ARCH-OWNERSHIP`: management owns peer
  advertisements and the MIB owns negotiated state; MAC/rate selection query
  it; PHY validates and represents the selected PPDU.
- `AR-WLAN-PHY-AUTHORITY`/`AR-WLAN-PHY-TIMING`: keep MCS, bandwidth, coding,
  NSS and duration calculation in mode/PHY objects rather than MAC tables.
- `AR-WLAN-MAC-EXCHANGE`, `AR-WLAN-MAC-SEQUENCE`, and
  `AR-WLAN-MAC-MULTIUSER`: one frame-sequence owner, one Block-Ack/reorder
  state owner, and an immutable scheduler plan before PPDU construction.
- `AR-WLAN-QUAL-TESTS`: every phase has focused tests plus legacy-mode
  regression. Do not update fingerprint baselines without separate approval.

Before code work begins, resolve sealing for every target under `src/inet/` and
obtain current-conversation approval for each sealed file.

## Phase 0 — Establish executable baselines

**Goal:** preserve the actual current behavior before extending it.

1. Run the existing VHT, HT, and HE unit tests, including
   `Ieee80211VhtCode_1.test`, `VhtMibCapabilities_1.test`,
   `VhtBeamformingMimo_1.test`, `VhtErrorModelLdpc_1.test`,
   `HtTxOpFs_1.test`, and the HE capability/sounding/MU-MIMO tests.
2. Add no feature assertions yet; record the smallest deterministic legacy,
   HT, VHT, and HE smoke configurations and their PCAP/fingerprint behavior.
3. Treat the old Wi-Fi-5 plan as historical: its claim that a VHT capability
   object exists is true, but it is not proof that management frames convey it.

**Acceptance:** a failing test is attributed before feature work starts; the
baseline confirms that `a`/`b`/`g`, `n`, and `ac` retain their current default
behavior with all new options disabled.

## Phase 1 — Make HT/VHT peer capabilities usable

This is the highest-value foundation. Current VHT fields in
`Ieee80211VhtCapabilities.h` and `Ieee80211Mib` have no management-frame data
path: `setPeerVhtCapabilities()` has no caller. HT has only a local LDPC flag.
Consequently, neither family can safely use peer-specific MCS/NSS/width/coding
choices.

### 1.1 Define compact, model-backed capability contracts

Add `Ieee80211HtCapabilities`/`Ieee80211HtOperation` and extend
`Ieee80211VhtCapabilities`/`Ieee80211VhtOperation` only with fields INET will
actually consume:

- directional RX/TX MCS/NSS limits and supported channel widths;
- LDPC, short GI, maximum A-MPDU length, and applicable HT/VHT protection;
- VHT SU beamformee/beamformer and DL MU-MIMO roles; and
- AP operating width/basic MCS information.

Model the result of intersection as directional local-TX/peer-RX and
local-RX/peer-TX contracts, as HE already does in
[`Ieee80211HeCapabilities.h`](../src/inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h).
Do not copy HE-only OFDMA/RU fields into HT/VHT.

### 1.2 Carry the contracts in management frames

Use the HE flow as the template, not a parallel protocol:

- add typed HT/VHT capability and operation element structures to
  `Ieee80211MgmtFrame.msg`;
- add paired encode/decode helpers beside
  `Ieee80211HeMgmtElements.h`, with exact serialized sizes;
- extend `Ieee80211MgmtFrameSerializer` symmetrically for beacon, probe,
  association, and reassociation frames; and
- have `Ieee80211MgmtAp` and `Ieee80211MgmtSta` advertise, retain, remove, and
  negotiate peer state through `Ieee80211Mib`, mirroring their HE calls to
  `setPeerHeCapabilities()`.

Simplified management must populate the same MIB state directly, as
`Ieee80211MgmtStaSimplified` already does for HE. This preserves its no-airtime
association model while avoiding a second capability store.

### 1.3 Gate all consumers on negotiated state

Update rate selection, aggregation, and transmitter mode selection to use the
directional contract when it exists; otherwise retain today's locally configured
fallback. An unsupported peer combination must be rejected or select a legal
legacy-compatible fallback at one PHY-validation boundary, never silently use a
sender-only setting.

**Tests first:** element serialization round trips; AP/STA and simplified-MAC
association intersection tests; asymmetric MCS/NSS/width and LDPC cases;
disassociation state removal; and a legacy association that contains no HT/VHT
elements. Add PCAP assertions for element presence, length, and values.

**Acceptance:** changing an AP or STA advertisement changes only the relevant
link's legal mode set; a peer never receives an HT/VHT PPDU it did not advertise
as decodable.

## Phase 2 — Complete HT/VHT PHY and frame-exchange fidelity

### 2.1 Share LDPC geometry, not HE dependencies

HE currently has a tested packet-level LDPC parameter calculation in
`Ieee80211HePhyCalculator.cc`; HT/VHT code still labels its coding chain
“TODO LDPC codes.” Extract only the amendment-independent codeword-length,
shortening, puncturing, repetition, and extra-symbol calculation into a neutral
IEEE 802.11 PHY utility. Keep HE PPDU/RU geometry in the HE calculator.

Then have HT and VHT mode/transmitter paths:

1. select LDPC only when Phase 1’s mutual capability permits it;
2. use the common calculation to derive coded payload capacity and duration;
3. retain BCC as the default and preserve the packet-level error-model fidelity
   choice; and
4. extend NIST/YANS validation only where the model has a distinct LDPC effect.

This reuse avoids a non-HE-to-HE dependency and keeps `Ieee80211Mode` as the
authority for legal HT/VHT combinations.

**Tests first:** table-driven HT/VHT BCC and LDPC duration/capacity boundary
tests across MCS, 20/40/80/160 MHz where applicable, NSS, GI, and payload sizes
near codeword boundaries. Retain the existing VHT LDPC tests and add equivalent
HT coverage. Test that one incapable peer forces BCC.

### 2.2 Implement real HT protected TXOP alternatives

`HtTxOpFs` currently declares L-SIG-protected, HT-NAV-protected, dual-CTS, and
initiator alternatives but represents them with the same basic sequence. Define
the state transitions, duration/NAV computation, eligibility rules, responses,
timeouts, and retries from the cited standard clauses, then implement distinct
frame-sequence branches. Reuse the existing `FrameSequenceContext`,
`FrameSequenceHandler`, `RtsCtsFs`, and protection/rate-selection contracts;
do not add a parallel MAC state machine.

Complete associated duration stubs in `TxopProcedure` and
`SingleProtectionMechanism` only when exercised by these exchanges. Keep
unsupported paths explicit until their state machine exists.

**Tests first:** one deterministic test per HT protection choice, including
timeout/retry and NAV duration; legacy RTS/CTS regression; Block-Ack and A-MPDU
boundary tests within a TXOP. Use event-log/PCAP assertions to prove the
on-air sequence rather than relying only on throughput.

**Acceptance:** each selectable protection method produces distinguishable
frames/timing and returns the EDCA/TXOP owner to a defined state after success
and failure.

## Phase 3 — VHT sounding, SU beamforming, and DL MU-MIMO

This phase is explicitly opt-in and must follow Phase 1. The initial result
should be a packet-level/MAC study model, not a new full waveform MIMO engine.

### 3.1 Reuse HE abstractions at their responsibility boundaries

HE already provides the right division of labor:

- capability conversion/negotiation:
  `Ieee80211HeCapabilities` and `Ieee80211HeMgmtElements`;
- deterministic CSI ownership/freshness: `HeMuMimoCsiManager`;
- sounding exchange ownership: `HeSoundingCoordinator` and `HeSoundingFs`;
- immutable user/resource selection: `HeDlSchedulerEqualSizedRUs` and the
  HE downlink plan; and
- PHY construction/validation: `Ieee80211HePhyCalculator` and
  `Ieee80211Transmitter`.

Extract only amendment-neutral concepts (for example a `MimoCsiManager`, a
typed CSI snapshot, and common sounding lifecycle helpers) if both HT/VHT and
HE can consume them without HE-specific fields. Keep VHT Group-ID, VHT feedback,
VHT PPDU format, and VHT Block-Ack behavior in VHT-named classes.

### 3.2 Deliver SU sounding and beamforming first

Add VHT sounding/action-frame representation and serializers, a deterministic
CSI update path, expiry/invalidation on disassociation and channel-width change,
and an explicit capability/configuration gate. Apply a documented packet-level
link benefit only after fresh CSI; do not claim waveform precoding. The effect
must enter through the PHY/error-model decision boundary, not by changing MAC
rate tables or payload bits.

### 3.3 Deliver DL MU-MIMO on the established contract

Implement an AP-only VHT scheduler that consumes negotiated capability snapshots,
queue state, fresh CSI, antenna/NSS limits, and deterministic address/TID tie
breakers. It must emit one immutable VHT MU transmission plan. PHY code validates
and builds the VHT PPDU; it does not choose users. Include group-management and
the required feedback/ack sequence before enabling multi-user scheduling.

Begin with a documented constrained subset (for example one bandwidth, bounded
NSS, two users, one acknowledgement strategy) and reject other combinations
clearly. Expand only after each subset is tested.

**Tests first:** capability gates; stale/missing CSI fallback to SU; CSI expiry;
sound/feedback frame serialization; deterministic user selection; invalid NSS
and incompatible peers; single- versus two-user PPDU accounting; failure/timeout
recovery; and a focused AP+two-STA simulation with PCAP/event-log evidence.

**Acceptance:** VHT MU-MIMO is impossible unless AP/STA capability, user option,
fresh CSI, and PHY limits all agree; disabled VHT and every pre-VHT mode retain
the former event trajectory.

## Phase 4 — Shared correctness required by the new features

These items benefit legacy, HT, and VHT and prevent the new capability path from
making optimistic assumptions.

1. Finish receiver data-service stages in
   `RecipientQosMacDataService`: A-MPDU deaggregation, header/FCS validation,
   Address1 filtering, duplicate removal, integrity/replay hooks, and explicit
   decryption boundary. Implement one stage at a time without conflating a
   packet-level FCS model with cryptographic security.
2. Complete `QosRateSelection` use of Supported/Extended Supported Rates,
   BSS basic rate sets, and HT operational MCS constraints. Feed it negotiated
   HT/VHT data from Phase 1 instead of duplicating capability state.
3. Complete only the FCS calculation/verification needed by the chosen packet
   fidelity model; expose it as a selectable fidelity improvement so existing
   declared-FCS scenarios remain reproducible.

**Tests first:** malformed/incorrect FCS, wrong receiver address, duplicate,
reorder-window, replay-hook, and aggregate member cases; rate choices with
asymmetric advertisements; and an unchanged default-mode regression.

## Delivery and validation gates

Each phase is a separately reviewable change series:

1. design note with IEEE clause references and exact changed-path seal status;
2. focused unit/module tests first;
3. implementation plus a build and the focused test suite;
4. one Cmdenv configuration/run with retained PCAP or event-log evidence where
   the feature changes an exchange; and
5. independent Wi-Fi regression and architecture review, including legacy mode
   and disabled-feature coverage.

Run the focused architecture check on both affected IEEE 802.11 subtrees. Do
not change fingerprint CSVs in this work without explicit approval. Update the
User’s Guide and the relevant example only after the feature has executable
coverage; document remaining deliberately unsupported subsets rather than
describing capability data types as complete behavior.
