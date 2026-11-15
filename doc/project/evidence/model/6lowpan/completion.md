# 6LoWPAN completion evidence

> **Kind:** ledger · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-completion.md](../../../../../plan/pending/6lowpan-completion.md), [release-validation.md](release-validation.md)

This ledger tracks P8–P15 separately from the archived P0–P7 acceptance. The user requested completion of the remaining packages on 2026-11-15. Passing one package does not establish later functionality. Existing first-release fixtures remain regression controls.

## P8 — Stateful contexts

Production owner: per-interface `LowpanLayer` and its bounded `LowpanContextTable`; codec: `LowpanIphcCodec`. Prefix lengths 0–128 are merged with inline/link-derived IID bits with context precedence. Source and destination IDs are independent; context 0 can omit CID. Prefix-based multicast uses a context prefix of at most 64 bits. Compression permission and decompression validity are distinct. Static XML configuration is optional and defaults empty.

Direct evidence: `LowpanStatefulContexts` unit vectors and the [native protocol fixture](../../../../../tests/protocol/6lowpan/stateful-contexts.md). The native fixture requires ten literal input frames, six exact IPv6 deliveries, two invalid-context drops, one replacement invalidation, two reassembly completions, five fragments and one real radio data transmission. It proves exact expiry rejection, stable first-fragment snapshots, FRAGN-first invalidation on prefix replacement, multicast reconstruction and receive-only source permission in emitted bytes.

The [P8 verification record](p8-verification.json) retains exact changed-source hashes, commands, exit statuses and log paths. All selected checks pass: 12 units, 62 modules and six protocol fixtures/twelve parameter runs, matching debug/release builds, feature-off build/five shared modules/three IEEE 802.15.4 fingerprint configurations, and core-without-UDP build/three units/one stateful protocol fixture. Original feature-state bytes and generated NED exclusions are restored. Raw logs remain under `/tmp/lowpan-p8/`. The initial unit enum spelling error was a fixture compilation error, not a protocol failure; its correction is retained in the raw attempt logs. The complete parent protocol fixture cannot execute unchanged against the parent API, so no runtime failing-parent claim is made.

## P9 — Extension NHC

`LowpanNhcCodec` transforms only the contiguous header prefix. The adaptation owner uses its expanded/compressed lengths for RFC 4944 coordinates and falls back to inline extensions when the complete compressed chain exceeds FRAG1. Registered chunks, serializers, printer and dissector cover extension EIDs 0–4 and nested IPv6 EID 7. Recursion is bounded at sixteen levels. Nested elided IIDs derive from the enclosing IPv6 fields without another U/L inversion.

Direct evidence is the [native protocol fixture](../../../../../tests/protocol/6lowpan/extension-nhc.md), plus independent wire and chain units. Eleven native input frames and three real senders produce ten exact IPv6 deliveries, three malformed-input drops and two separately checked fallback first fragments. Both a prohibited 262-byte Routing body and a legal 254-byte body exceeding the first-frame budget remain inline. Mixed extension/UDP and nested chains have literal unit vectors.

The [P9 record](p9-verification.json) retains source hashes and exact commands: debug/release builds, fifteen units, sixty-two module fixtures/eighty-four parameter runs, seven protocol fixtures/thirteen parameter runs and three CSMA fingerprint configurations pass. Two units supplement P9 with generic checksum/ND-wire controls; they do not establish registration policy. Initial module failures exposed a shared IPv6 decoded-Fragment-header lifetime bug and one obsolete wire assertion. The separate IPv6 prerequisite retains the shared chunk through reassembly, and a direct serialized-fragment fixture plus every affected case passes. Independent P9 review found no actionable runtime defect; its introspection-placement and legal-chain-budget coverage requests are resolved. The final combined feature matrix remains pending.

## P10 — Local server stage

The native ND owner now accepts local RFC 6775 ARO registrations with `nativeNdRole="router"` and IPv6 forwarding enabled. Module-wide capacity, owner identity, actual native address and absolute expiry are separate state. Successful registrations install owner-specific native bindings and /128 routes. Duplicate, cache-full, refresh, zero lifetime, exact expiry and lifecycle paths preserve other owners and static configuration. Native control replies compute their wire checksum. The default static role is preserved.

The [native server fixture](../../../../../tests/protocol/6lowpan/neighbor-registration.md) requires sixteen injected frames, eleven actual radio NAs, one data packet delivered through a learned route and four malformed-input drops. It checks error delivery to an ARO owner different from the sender, reserved receive/transmit rules, a static-conflict address separate from the learned links, refreshed route expiry, crash/restart and graceful stop/restart. The [stage record](p10-stage1-verification.json) retains exact hashes and commands: matching debug/release builds, one protocol fixture and five module fixtures/six parameter runs pass. Independent review reports no unresolved finding. The owned-domain storage prerequisite has its own direct module fixture and commit before packet handling.

P10 remains open: host RS/RA discovery and refresh, 6CO/ABRO distribution and multihop DAR/DAC have not been established by the server stage. The final combined feature matrix also remains pending.

## P10 — Native router advertisements

Advertising-enabled native router interfaces now validate RS input and send unicast RAs with native SLLAO and off-link PIOs. Pending requester tuples are bounded and coalesced, responses use the simulation RNG with delay up to two seconds, and prefix lifetimes are sampled at emission. Advertising permission is checked before queuing and again at transmission. Stop/crash clear the pending responses and destruction cancels their timer. Receiving an RS does not change an existing native binding.

The [router discovery fixture](../../../../../tests/protocol/6lowpan/router-discovery.md) checks eleven native RS frames, three real RAs carried by six radio fragments, three malformed drops, disabled advertising, pending-response coalescing, both lifecycle cancellation paths and restart. Prefix addresses and absolute/relative/infinite lifetimes are checked against actual emission times. The [router record](p10-router-verification.json) retains matching debug/release builds and two protocol fixtures/two runs; independent review confirms the advertising-permission and randomized-delay findings resolved. The separate [IPv6 validity prerequisite](ipv6-validity-verification.json) passes its focused exact-deadline unit and both builds, supplying read-only lifetime checks for the following host client.

Host discovery/registration generation and the distribution/multihop stage remain in progress; this responder alone does not complete P10.

## P10 — Ordinary native neighbor solicitations

Native registration routers distinguish ordinary Neighbor Solicitations from ARO requests. Valid unicast probes may omit SLLAO and receive a solicited NA without TLLAO or Override. Multicast probes receive native TLLAO, while valid unspecified-source DAD probes receive an all-nodes native broadcast NA with Solicited clear. Target lifetime and tentative status are checked before replying. These replies preserve existing registrations and use validated native receive identities rather than the Ethernet ND decoder.

The [standalone record](p10-native-ns-verification.json) verifies the responder before the host client in an isolated checkout, with the fixed ND option enum prerequisite present. Matching debug/release builds and two debug protocol fixtures/two runs pass. The registration fixture observes twenty-one inputs, eleven ARO replies, two ordinary unicast replies, two peer receptions of the DAD broadcast, one learned-route data delivery and six malformed drops. The release consumer's printed ARO/data/drop/count sequence is independently checked; disabled release ASSERTs do not establish its per-field checks. Independent source review is clean, and scoped architecture, naming and source-protection checks pass. The [shared enum repair](nd-option-values-verification.json) separately proves all 256 values representable and unknown options 99/255 reserializable without changing their wire values.

## P10 — Native host discovery and registration

The native M=0 host role owns router discovery, EUI-derived SLAAC, address registration and refresh through an ND-owned helper. Router, address and accepted-registration deadlines are independent. Finite manual addresses and the RFC 4862 two-hour SLAAC rule are preserved, including large finite wire lifetimes beyond the simulation clock horizon. Ordinary host data requires a valid, non-tentative registered source; incoming local delivery also checks usable assignments while preserving valid ordinary-interface aliases. Registration control remains usable during bootstrap. Failed or unrouted native data drops without a queued replay.

The [host fixture](../../../../../tests/protocol/6lowpan/host-registration.md) covers six scenarios: normal registration and refresh, cache-full, duplicate rejection, an unreachable registrar, unsupported M=1 assignment and independent discovery on two native interfaces. Literal success and duplicate NAs arrive at an address's exact deadline before its timer; neither can revive the address or remove another usable registration. Ordinary native NS replies are observed over the radio. The sibling-interface case withdraws one router, disables its advertisements and delivers an unknown zero-lifetime RA after backoff, requiring continued solicitation while the other interface remains usable.

The [host record](p10-host-verification.json) retains fifteen exact source/test/document hashes, commands, seed zero, durable logs and observed packet counts. Matching debug/release builds, three protocol fixtures/eight runs and five module fixtures/six runs pass. Independent review confirms all three reported mechanisms resolved; scoped architecture/naming and whitespace checks pass. Release-mode behavioral execution and the combined feature matrix remain unexecuted. This completes the selected M=0 host stage; authoritative context distribution and routed duplicate registration remain separate P10 work.

## P10 — ND receive validation before dispatch

`LowpanIpv6` rejects malformed known ND and DAR/DAC messages before inherited IPv6 performs a strict chunk peek. Fixed minimum lengths and permitted inspection of incorrect/incomplete decoded headers turn these inputs into observable malformed drops. Other ICMPv6 types and valid ordinary ND retain their inherited path. Generic IPv6 and sealed packet code are unchanged.

The [static Ethernet fixture](../../../../../tests/protocol/6lowpan/nd-wire-validation.md) needs no authority helper. Its unchanged inputs fail against the source-equivalent parent at event 7, time 0.01000789 seconds, with the strict-peek exception. Applying only the guard passes all eight controls: five malformed drops, two unsupported valid DAR drops and one ordinary NA. The [standalone comparison record](p10-nd-wire-validation-verification.json) retains both runs, matching debug/release builds, four fixed debug protocol fixtures/nine runs, raw hashes and clean independent review. Release behavioral assertions and the combined feature matrix remain unexecuted.

## Remaining packages

P10–P15 remain in execution order under the completion plan. Optimized/extended ND, mesh-under, paging, virtual fragment forwarding and selective recovery are not inferred from header compression support.
