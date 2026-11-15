# 6LoWPAN first-release validation

> **Kind:** ledger · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-implementation-plan.md](../../../../../plan/pending/6lowpan-implementation-plan.md), [provenance.md](provenance.md), [encoding-profile.md](encoding-profile.md)

P0's standards provenance is recorded. P1–P6 production source is implemented. P7 remains open: the selected tests pass, but the complete first-release matrix still has the native production-path gaps identified below. A pass of a helper, compatibility fixture, capture decoder or peer vector has the scope of that observation; it does not silently complete a different acceptance row.

## Source and execution environment

The production source is commit `6a11f885acd503273abd53ab47d546f12b38b41e`, tree `067ce2a25597f99d2d38f2a2d78a32c149380a15`. This continuation changes tests and evidence only. The unrelated local `AGENTS.md` edit is retained. Commands run from the INET root, except the three examples, which run from `examples/lowpan`. Behavioral checks use debug mode with assertions, run/seed 0 unless the fixture names a finite parameter iteration. Libraries are rebuilt after each feature-profile change; test-only changes reuse the matching library and rebuild their fixture executables.

The installed runners are `opp_run_unit_tests`, `opp_run_module_tests`, `opp_run_protocol_tests` and `opp_run_fingerprint_tests`, loaded with `--load . -p inet`, from `/home/miguel/omnetpp-6.4.0aipre2/.venv/bin`. Raw logs, generated-input hashes and exact command records are retained under `/tmp/lowpan-p7-20261115/`. Test source and the [protocol reviewer document](../../../../../tests/protocol/6lowpan/native-reassembly.md) provide the durable reproduction when temporary artifacts are absent. This is deterministic boundary evidence, not a statistical claim across seeds or channel conditions.

## Acceptance mapping

Names below are fixture basenames. `PASS` means the stated observation ran; `PARTIAL` keeps the uncovered portion of the plan's row explicit. Native MAC input injection observes decoding, indications and adaptation; it bypasses radio propagation. Full sender/radio fixtures and external byte checks are identified separately.

| Plan row | Direct evidence | Current scope and remaining gap |
| --- | --- | --- |
| P1 metadata | `Ieee802154ReceiveMetadata`; `LowpanNativeReceive` | PASS: legacy production receive metadata and native byte filtering. Compatibility and native headers remain distinct. |
| P2a identity/mapping | `LowpanNativeAddress`, `LowpanLinkAddressMap`, `LowpanLinkDomain`, `LowpanDomain*`, `LowpanNativeDomain*` | PASS for the selected static-domain, alias and rejection cases; mapping results do not establish native wire encoding. |
| P2a payload/tags | `LowpanOpaquePayload`, `LowpanPayloadProtocolMismatch`, `LowpanPayloadUnknownProtocol`; `LowpanNativeReceive` | PASS: configured legacy dispatch boundary and native MAC receive. The new protocol case receives bytes without TX request tags. |
| P2a budgets | `LowpanNativeFrame`, `LowpanNativeEnvelope`, `LowpanNativeRetryEnvelope`, `LowpanQueuedEnvelope` | PASS for selected preparation, invalidation and retry cases. Native unicast/broadcast capacity is 104/110 bytes; capture lengths include the MAC header and FCS. |
| P2b dispatch | `LowpanUncompressedDispatch`, `LowpanMalformedReceive`; `LowpanNativeReassembly` | PASS: native truncated `7f`, unknown dispatch, empty payload, unsupported page switch and reserved stateful destination reject; valid literal IPHC reaches IPv6. |
| P2b temporary no-fragment limit | Historical P2b evidence in `coverage.md` | Superseded by P3 in the current implementation. The final source must fragment supported larger packets, not keep the temporary `fragmentationUnavailable` behavior. |
| P2b/P6 MTU and multicast | `LowpanNative1280`, `LowpanNativeUdp`, `LowpanNativeUncompressedExchange` | PASS: 1280-byte IPv6 MTU, extended unicast, short broadcast and native PAN/address fields in selected exchanges. |
| P3 frame/alignment boundaries | Expanded `LowpanFragmentation`; `LowpanNativeFragmentBoundaries`; independent captures | PASS: helper checks all 7,430 combinations of 40–1280-byte size, 104/110-byte capacity and the three header-coverage profiles. Four native sender/radio runs deliver all 4,964 datagrams byte-exactly, with serialized lengths, original-coordinate offsets and every alignment edge checked. UDP/NHC has separate selected cases below. |
| P3 reassembly | `LowpanReassembly`, `LowpanPeerReceive`; `LowpanNativeReassembly` | PARTIAL: native FRAGN-first, interleaved tags 65535/0, exact duplicate and byte-exact completion pass. Reusing an identical tag across receiving interfaces/PANs and distinct senders still needs a dedicated native protocol case. |
| P3 invalid input | `LowpanReassembly`, `LowpanFragmentation`; `LowpanNativeReassembly` | PASS for native overlap restart, conflicting-duplicate quarantine, non-final misalignment, out-of-range final range, truncated fragment headers and subsequent recovery. A separate-size context cannot contribute bytes to the original datagram and expires independently. |
| P3 resources/lifecycle | `LowpanNativeReassembly`, `LowpanNativeLifecycle`, `LowpanNativeTurnaroundLifecycle`, `LowpanReassemblyExpiry`, `LowpanFragmentedLifecycle` | PARTIAL: native context/byte limits are isolated in separate runs, reservations are reused, and exact duplicate/expiry times are observed. Lifecycle fixtures remain complementary; a native protocol sequence with pending RX plus teardown/restart must complete the row. Association/disassociation is outside the static-PAN model. |
| P3 tag wrap/reuse | `LowpanDatagramTags`, `LowpanReassembly`; `LowpanNativeReassembly` | PARTIAL: allocator/helper boundaries and receiving tags 65535/0 pass. A native production sender sequence covering wrap, guard exhaustion/expiry and restart is still owed. Wire-identical nonconflicting generations remain intrinsically indistinguishable. |
| P4 IPHC | `LowpanIphc`, `LowpanIphcAcceptMatrix`, upgraded `LowpanIphcReceiveMatrix`; new native protocol literal | PASS: native MAC execution of all 1,280 TF/HLIM/SAM/DAM/multicast/unspecified-source/CID receive combinations. This is acceptance of literal bytes; it does not claim that the compressor selects every accepted encoding. The sender sweep independently checks the selected elided stateless prefix. |
| P4/P5 compressed fragmentation | `LowpanCompressionBudget`, `LowpanIphc1280`, `LowpanNative1280`, `LowpanNativeRouteOver` | PASS for selected placement/fallback and original-coordinate cases. The controlled small-budget fixture is separate from the native minimum 104-byte budget, which accommodates the supported compressed header chain. |
| P5 UDP | `LowpanUdpNhc` unit/module; `LowpanNativeUdp`; `LowpanNativeReassembly`; external capture/peer vectors | PASS for four port modes, carried checksums and selected fragmented/unfragmented unicast/multicast. Native MAC injection rejects every proper truncation of the four UDP NHC headers, all four checksum-elision forms and an unsupported NHC identifier. Checksum elision remains unsupported. |
| P5 routing/IP MTU | `LowpanNativeRouteOver`, `LowpanNativeTwoFlows`, `LowpanNativeForwardedPtb`, `LowpanNativeIpv6Fragment`, `LowpanQueuedRouteOver` | PASS for selected two-PAN/two-flow, forwarding, source-fragmentation and queued-egress stimuli. These are named cases, not all route/topology combinations. |
| P6 wire | `LowpanNativeFrame`, `LowpanNativeReceive`, native ACK/retry/deadline/envelope fixtures; independent capture decoder and pinned peer | Selected native data/ACK bytes, FCS, lengths and checksums pass. Global-address peer vectors and fully elided EUI address disagreement are reported separately below; neither is a live peer MAC/radio exchange. |
| P7 ordinary ND | `LowpanMixedNeighbourDiscovery` plus documented static native neighbors | Out of release scope. The compatibility/mixed fixture does not establish dynamic native ND or RFC 6775 registration. |

## Executed campaign

The first enabled-profile run passes 11 unit and 65 module fixtures with these exact selectors:

```sh
opp_run_unit_tests --load . -p inet -m debug --no-build \
  -f '(Lowpan.*|Icmpv6PseudoHeader)\.test$'
opp_run_module_tests --load . -p inet -m debug --no-build \
  -f '(Lowpan.*|Ieee802154ReceiveMetadata|Icmpv6LocalChecksum|Icmpv6ExtensionChecksum|ICMPv6_delivery|IPv6_packet_too_big|MLD_smoke)\.test$'
```

Logs: `enabled-unit.log` and `enabled-module.log`. The expanded fragment-boundary unit then passes in `fragment-boundaries.log`. Final enabled-profile confirmation passes all 11 units and 65 modules in `final-unit.log` and `final-module.log`, including the upgraded native IPHC matrix. Both native protocol fixtures pass: `final-protocol-current.log` records the four sender/radio parameter runs and the final receive fixture's two parameter runs, 61 injected frames and nine exact deliveries per receive run. Source hashes and per-command results are retained in [release-verification.json](release-verification.json) and the artifact directory's `verification.json`. Commands and assertions are in the [reviewer document](../../../../../tests/protocol/6lowpan/native-reassembly.md).

The initial protocol attempt had a NED namespace setup error, retained as `native-protocol-initial-setup.err`; it supplied no behavioral evidence. An intermediate inconsistent-size stimulus also exceeded the run-1 byte budget; `native-protocol-expanded-capacity.err` retains that fixture failure. The corrected size isolates reassembly identity as intended. No production correction or expected-failure declaration was used.

Feature-disabled verification uses `opp_featuretool disable LowpanExamples LowpanTests Lowpan`, then `make MODE=debug -j8`. The build passes (`feature-off-build.log`). Five selected legacy/shared module fixtures pass (`feature-off-module.log`):

```sh
opp_run_module_tests --load . -p inet -m debug --no-build \
  -f '(Ieee802154ReceiveMetadata|ICMPv6_delivery|IPv6_packet_too_big|Icmpv6LocalChecksum|Icmpv6ExtensionChecksum)\.test$'
opp_run_fingerprint_tests --load . -p inet -m debug --no-build \
  -w '^examples/wireless/nic$' \
  -c '^CSMAWith(UnitDisk|ApskScalar|ApskDimensional)Radio$' -r '^0$'
```

All three fingerprint configurations pass unchanged at 100 s with the stored `tplx`, `~tNl` and `~tND` ingredients (`feature-off-fingerprint.log`). No baseline update is performed.

The core-without-UDP profile enables `Lowpan` alone, then runs `opp_featuretool disable -f Udp` and `make MODE=debug -j8`. Build, the same 11-unit selector, and `opp_run_module_tests --load . -p inet -m debug --no-build -f '(LowpanIphcReceiveMatrix|LowpanNativeCcaPulse)\.test$'` all pass (`core-{build,unit,module}.log`). This includes all 1,280 native receive combinations without the UDP transport feature.

The original feature-state bytes are restored after the campaign, and the generated NED exclusions are reconciled through the feature tool. Both `make MODE=debug -j8` and `make MODE=release -j8` pass (`restored-{debug,release}-build.log`). A first example attempt exposed stale generated exclusions after byte-only feature-state restoration; `example-single-hop.log` retains that setup failure. After regeneration, all three documented examples complete with exit 0: single-hop IPHC, single-hop uncompressed, and route-over (`example-final-{single-hop,uncompressed,route-over}.log`, commands in `examples-final.json`). Simulation completion alone is not used as a payload assertion; the corresponding module/protocol fixtures establish delivery.

## Independent byte validation

TShark 4.6.4 is unpacked with its required runtime libraries under `/tmp/lowpan-p7-20261115/tshark/`, without system installation. Its unrelated Falco plugin cannot load in this isolated deployment; the built-in IEEE 802.15.4, 6LoWPAN, IPv6, ICMPv6 and UDP dissectors execute. The existing `check-capture.py` runs with UDP checksum validation enabled on two native ICMPv6 captures and four native UDP captures regenerated by the enabled module campaign. All six pass; their hashes, lengths, frame and checksum counts are in [release-captures.tsv](release-captures.tsv). Maximum observed PSDU length is 127 bytes. These are bounded frame/checksum observations, not complete conformance or live peer interoperability.

Reproduce after the module campaign with `python3 doc/project/evidence/protocol/6lowpan/checks/check-capture.py tests/module/work/LowpanNative1280/native-{a,b}.pcapng tests/module/work/LowpanNativeUdp/native-udp-{0,1,2,3}.pcapng` using TShark on `PATH`. The copied captures and raw output remain under the execution artifact directory. An initial invocation raced the fixture's regenerated UDP output; that setup error is retained separately in `captures-initial.err` and is not a packet-validation failure.

The [peer README](../../protocol/6lowpan/checks/peer/README.md) pins ns-3 `5e35cfbc28bd43aa9ceb0d1edea44078d6e76830`. It is retrieved from the canonical GitLab repository; the GitHub mirror used in the first attempt did not supply it. The clean checkout is configured with `./ns3 configure --enable-modules=sixlowpan --disable-examples --disable-tests --build-profile=default`, then built with `./ns3 build sixlowpan -j 6`. Build/configure logs are retained; CMake/Ninja are isolated under the artifact directory. Peer production source is unchanged.

`doc/project/evidence/protocol/6lowpan/checks/peer/run.sh /tmp/lowpan-p7-20261115/ns-3 /tmp/lowpan-p7-20261115/peer` passes (`peer.log`). Fresh route-over exports match both committed INET vector files exactly, and `LowpanPeerReceive` passes in the final enabled module campaign. Thus the fragmented global-address UDP vector exchange is reproduced in both directions. This is serialized adaptation-vector exchange, not a live peer MAC/radio test.

The peer's elided-address decode is separately executed and still differs at IPv6 offsets 16 and 32: expected `0x02`, observed `0x00` ([retained result](peer-elided-result.json)). It exactly reproduces the saved incompatible output. The cause is established in the pinned `src/network/utils/ipv6-address.cc`, `MakeAutoconfiguredLinkLocalAddress(Mac64Address)`, which copies the eight EUI bytes without U/L inversion. INET's native protocol fixture independently passes the inversion required by RFC 6282 section 3.2.2. This is a characterized peer incompatibility; it remains a failed external comparison, not an expected-pass result or a reason to change INET's mapping.

## Remaining P7 work

Keep P7 unchecked and this plan in `plan/pending/` until the partial rows above are closed. Add explicit native protocol fixtures and reviewer explanations for (1) identical tag/address/size tuples across distinct senders and receiving interfaces/PANs, (2) pending reassembly across teardown/restart with late old fragments, and (3) production sender tag wrap, guard exhaustion/expiry and restart. Existing helper/module evidence remains useful but does not discharge those exact protocol sequences. The peer's incompatible elided-address result must remain visible alongside the normative INET expectation. Do not enable stateful IPHC, optimized ND, security, association, short unicast, mesh-under or advanced fragmentation as part of closing these first-release gates.
