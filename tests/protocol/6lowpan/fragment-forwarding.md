# Native route-over fragment forwarding

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [publication plan](../../../plan/pending/6lowpan-publication-plan.md)

Publication association: [C38](../../../plan/pending/6lowpan-publication-plan.md#c38); donor `063659d694`. The retained setup, assertions and parent explanation below describe this behavior. Original run counts, PASS statements and temporary artifact locations refer to the donor cohort. New-prefix results are recorded separately in the publication verification manifest; none is inferred from those historical statements. Exact-parent runtime execution is not claimed unless that manifest records it.


`LowpanFragmentForwarding.test` exercises an opt-in RFC8930 forwarding profile
through real native MACs and radios. Configuration declarations and supported
ranges live in [LowpanLayer.ned](../../../src/inet/linklayer/lowpan/LowpanLayer.ned).
The ingress Layer owns dispositions, original-coordinate ranges, fixed deadlines
and closed-identity protection; the selected egress Layer owns its shared tag
allocator, prepared native envelope and lower-completion correlation.

```mermaid
flowchart LR
    A["A: paced source"] <-->|"PAN4660: actual native radio"| BI["B ingress: allowlists A"]
    BI -->|"IPv6 route; original offsets; swapped tag"| BO["B egress: ACK-gated output"]
    BO <-->|"PAN9029: separate actual native radio"| C["C: ordinary reassembly and IPv6/UDP"]
```

A cannot hear C. Static native neighbors and IPv6 /128 routes define this data
composition; no multihop ND claim is made. B has the actual standard ICMPv6 hook.
Its UDP transport is disabled except the explicit UDP-hook fallback control;
C retains its real computed-checksum UDP hook. The normal source is a1280-byte
IPv6/UDP datagram. Independent byte comparison at C checks only outer Hop Limit
changes from64 to63, while the real UDP sink independently receives the payload.
One general-header case builds literal IPv6/HBH/UDP bytes, calculates the UDP
checksum independently, and compresses a56-byte first header with existing NHC.
Its forwarded outer header has Next Header inline; all subsequent bytes retain
their original positions.

## Supported composition and identity protection

Forwarding requires the native paced-source profile and excludes Mesh. An
administrative ingress allowlist declares the actual previous native peer's
compatible data-tag protection; no capability is inferred from remote modules or
IPv6 Hop Limit. The declared deployment hop bound and effective simulation-clock
durations must satisfy the local admission checks. This is a deployment contract,
not discovery of an arbitrary path's timing. Empty lists admit no optimized input
and allow egress-only participation.

The listed forwarding profile drops orphan FRAGN, following RFC8930's
forwarding-node rule. The selected restriction also applies before local/control
classification. This includes
fragmented control whose FRAG1 has not arrived: reordered listed-peer ND can be
lost. A positively identified ND/control, incomplete chain or forwarding-ineligible
FRAG1 creates sticky ordinary reassembly with its existing expiration; it never
acquires the longer data history. Unlisted, broadcast and mode-off paths retain
ordinary FRAGN-first behavior. The selected controls include all seven ICMPv6 ND/
registration types133–137 and157/158. Their partial datagrams test adaptation
classification/retention, not successful ND policy processing.

Eligible local data uses normal IPv6 delivery and hook processing while retaining
data-identity protection. Forwarded data has one lower original outstanding per
flow, waits for a matched first ACK, and observes the ACK-relative gap at the actual
egress radio. Each retained range keeps its ingress deadline. Success means local
next-hop native completion; it does not acknowledge final-destination delivery.
Completion, failure and lifecycle cancellation free active bytes while preserving
the original finite identity history. Duplicate inputs do not renew that history.

Only the exact associated standard ICMPv6 hook is ignored by the forwarding-hook
query: its received-path hooks are inert, including the InterfaceInd short-circuit
in postrouting. Custom hooks, ICMP subclasses and the standard UDP checksum hook
force ordinary IPv6 processing before output. A hook registered after forwarding
starts aborts unsent work. Valid routes whose egress is outside the optimized
profile also fall back; absent or exactly expired routes generate the proper
public ICMPv6 error using a real input-interface indication and available quote.
The IPv6 owner's actual native source-usability predicate is shared with ordinary
egress and checked on every selected path/continuation. A valid mixed
host-registration/forwarding configuration therefore falls back when its source
is unusable; ordinary IPv6 retains the observable source refusal. Loss of that
eligibility after output cancels unsent forwarding rather than changing methods.

The egress's real compression setting is honored. IPHC encodes the decremented
outer header with Next Header inline; `none` emits0x41 plus the raw outer header.
Deep/capacity-ineligible transforms fall back before any output. After output,
path changes, failure or deadline expiry cancel instead of switching methods.
Exact FRAG1 bytes and nonfirst range shapes identify duplicates; repeated
nonfirst payload equality is deliberately not checked.

## Direct cases

Every run uses seed0. The fixture's `case` iteration is the run number.

| Runs | Stimulus and causal observation |
| --- | --- |
|0–1 | Real paced1280B source; early B output precedes last A fragment. C gets byte-exact IPv6 and real UDP. A13 fragments become B14, or B27 under a64-byte adaptation cap. The MAC MTU/envelope is unchanged. |
|2–3 | Unlisted ingress and a real custom IPv6 hook each require B reassembly first. Hook counters prove generic pre-routing/forward processing. |
|4 | Register a custom hook after the first output: one forwarded frame, no completion, no later output. |
|5,10 | Destination radio unavailable, or both actual first ACKs discarded by a fixture-only inherited MAC: one logical first output, two actual attempts, one failure, no destination completion. |
|6,24,42 | Too-small first-frame cap, pending-byte preflight bound or zero carry allowance selects ordinary reassembly before output; normal IPv6/source pacing still delivers. |
|7,25 | No route or route expiry at the source event: no forwarded frame, one real checksum-valid Destination Unreachable reply. |
|8,12 | Real standard UDP hook at B, or valid route to a forwarding-disabled native egress: ordinary IPv6 path succeeds. |
|9 | Re-send all saved B output through the actual B radio at20s: C's local data history prevents duplicate adaptation/UDP delivery, despite its real UDP hook. |
|11 | Actual egress `headerCompression=none`:0x41, raw HL63, original-coordinate coverage and byte-exact UDP delivery. |
|13 | Literal valid-checksum HBH/UDP NHC first header, original width56B; reconstructed header/tail repartition and real UDP delivery. |
|14–17 | Exact first duplicate is harmless; conflicting first and a gapped tail cancel; a different-tag orphan tail drops without disturbing the live transfer. Adverse inputs traverse the actual A MAC/radio. |
|18–19 | Slow egress plus64-byte cap makes aggregate-byte or pending-range admission fail. Exactly one RESOURCE_LIMIT abort is required, with no destination completion. |
|20–22 | Equivalent route replacement preserves delivery; route removal or changed next-hop/native tuple after first output cancels. No borrowed production route pointer is retained. |
|23 | A second identity is refused while the sole record is live; the incumbent still succeeds. |
|26–32 | Each ND/control type's first-only datagram expires normally; the same identity is admitted again after61s and expires again. No120-second data tombstone is installed. |
|33 | Incomplete HBH classification uses the same sticky ordinary60s behavior. |
|34–36,44 | Listed orphan control/data tails drop; the local partial context expires once. Unlisted and mode-off FRAGN-first controls complete ordinary local reassembly. |
|37–38 | Local identity cannot redeliver at60s, but can after120s; duplicates do not extend the deadline. A completed tombstone also counts against the one-record capacity. |
|39 | Two concurrent identities share one egress allocator, get distinct live outgoing tags, preserve per-flow ACK-relative gaps and deliver two byte-exact320-byte datagrams. |
|40,45 | Hop-limit exhaustion suppresses error-to-error, while an informational ICMP quote produces one real checksum-valid Time Exceeded reply. |
|41 | Broadcast partial input remains ordinary, with two60s expirations and no data history. |
|43 | A short ingress residence expires retained carry before its next scheduled output: one first frame, one lifetime-expired abort, no completion. |
|46 | Local overlapping ranges restart ordinary reassembly, but cannot extend the original90s active bound or120s history. The original context is cancelled by91s, a tail at118s cannot revive it, and the same identity successfully reassembles after150s. |
|47 | Seven nested NHC/IPv6 headers reconstruct280 original header bytes from a native-size first frame. The bounded carry preflight selects ordinary reassembly before output, with normal repeated60s retention; no nested/tunnel application delivery is claimed. |
|48 | Real B host-registration mode with forwarding enabled has a valid native route but no accepted source. B reassembles normally and the common IPv6 guard drops that exact A→C datagram once with ADDRESS_RESOLUTION_FAILED; bootstrap ND cannot mask zero forwarded data and zero C UDP delivery. |
|49–51 | A conflicting datagram size cancels the live identity. A repeated nonfirst range with the same shape but changed payload is ignored while exact original data still reaches C. A changed length at the same offset cancels instead. All three adverse frames traverse the actual source MAC/radio. |

The five separate
[lifecycle and synchronous observation cases](fragment-forwarding-lifecycle.md)
cover egress-MAC STOP/CRASH between frames, observer-driven cancellation/borrowed
frame lifetime, and an ingress Layer remaining DOWN beyond its history timer.
The last case requires exactly one canonical reassembly-expiry observation at C.

## Verification and limits

The full14-source checkpoint passes22 debug protocol fixtures/204 simulations,
including52 main and five lifecycle cases, plus19 focused units. Its matching
release passes eight protocol fixtures/130 simulations. All128 structured protocol
counter rows match debug independently. Two older release fixtures provide normal
completion only. Both modes pass14 mapped module fixtures, comprising38 completed
simulations plus one expected existing initialization diagnostic. The20 native
completion counter rows also match between modes. Final private-name/dead-field
cleanup is checked by an exact source transformation and fresh matching mode
builds; the52 main and five lifecycle cases are repeated in both final modes.
Unchanged mapped/core results retain their pre-cleanup source/library snapshot
and the explicit mechanical equivalence rather than claiming a new broad run.
Release builds disable
`ASSERT`; release claims are normal completion and compared observations, not
execution of debug assertions.

At the preserved14-source checkpoint, a fresh NoUDP production build and seven
codec/mechanism units plus two actual
core module controls PASS, including1,280 native IPHC receive combinations and a
latched CCA pulse. Feature-tool regeneration and semantic/exact-byte checks restore
the original feature/NED state before both final enabled builds. Architecture,
naming, explicit source-seal and scoped diff gates PASS. The original PATH-based
C++ command could not run because `clang-tidy` was absent from PATH. A subsequent
cached-tool42-source gate actually ran with exit1 and no compile errors; its
scope, diagnostics and triage are recorded separately. Its two new private-name
violations are repaired; two older constant-name violations remain scheduled
final-audit work. Warning-bearing execution is not relabeled PASS.
Manual ownership,
numeric, lifecycle and INET contract self-audit accompanies independent review.
Exact commands, raw
outputs, generated inputs/binaries and source/library hashes are in
[the verification record](https://github.com/mgonzalezlopezudc/inet/blob/9908c007bdb1eda9d74f6ca1f99fe1cae33fbc61/doc/project/evidence/model/6lowpan/p14-vrb-verification.json).

The literal controls use public NativeBoundary preparation and actual native radio
transmission. They deliberately bypass the source fragmenter's admission/pacing
only to construct adverse input, concurrent identities and partial control.
Ordinary local Next Header59 controls may produce generic ICMP Parameter Problem
replies; the checker distinguishes that return traffic from optimized data.
The main live application topology depends on UDP at A/C; it cannot validate a
NoUDP application configuration. Separately mapped core controls and a fresh
NoUDP production build cover that feature boundary.

No claim is made for arbitrary deployment hops/timing, final-destination ACKs,
reliable repair, unbounded duplicate history, or equality
of duplicate nonfirst payloads. Module recreation loses finite history. The stage
is implemented together because admission/disposition, header reconstruction,
owned ranges, real egress callbacks and fixed identity protection form one
production state machine; an unused parser or buffer prerequisite would have no
independently supported forwarding behavior.
