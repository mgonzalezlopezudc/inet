# 6LoWPAN implementation evidence

> **Kind:** ledger · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [implementation plan](../../../../../plan/pending/6lowpan-implementation-plan.md), [features](../../protocol/6lowpan/features.md)

Execution began 2026-09-19 at `14df0d93a48cd0f4c619d3a16066809c6f40243d`, with a clean
working tree. P1–P6 implementation has focused debug, release-build and independent
wire/vector evidence. P0 provenance and the full P7 release matrix remain open.
Earlier milestone sections below are historical; the final sections supersede their status.
Status below distinguishes executed checks from planned work.

| Package | Behavior / check | Owner | Category | Status |
| --- | --- | --- | --- | --- |
| P1 | Legacy receive source, destination, interface and protocol | Ieee802154Mac::decapsulate | Production module receive + protocol observation | covered, PASS |
| P2a | Native aliases, collisions, PAN isolation, opaque RX and budgets | IEEE 802.15.4 types; compatibility adapter | Unit + module | partial coverage, see executed checks below |
| P2b | 0x41, bounded dispatch, MTU and fitting exchange | Lowpan processing module | Unit + production module + standalone simulation | covered for P2b; see executed checks |
| P3 | Original offsets, reassembly identities, timer, overlap and limits | Fragment planner; reassembly table | Unit + protocol + lifecycle module | implemented; focused PASS, see remaining evidence below |
| P4 | Encode/accept matrix and compression/fragmentation interaction | IPHC codec | Independent byte vectors + production protocol | supported literal receive matrix and placement fallback PASS |
| P5 | UDP checksum/ports; per-hop reconstruction; IPv6 MTU behavior | NHC codec; IPv6 integration | Unit + two-link protocol | NHC, two-flow route-over, queued egress and IPv6 MTU cases PASS |
| P6 | Native data/ACK subset, filtering/retries, FCS and external bytes | IEEE 802.15.4 MAC/serializer | Module + serialization + external peer | implemented; bounded native and peer evidence PASS, see limits below |
| P7 | Native first-release rerun, features, docs; static neighbors | Interface integration | Build + protocol | selected native matrix, examples and both builds PASS; provenance and broader peer limits remain open |

The [standards-only procedures](../../protocol/6lowpan/checks/data-plane.md) define
packet expectations. The plan's §5 defines detailed boundary/lifecycle stimuli. The
[encoding profile](encoding-profile.md) pins separate encoder and decoder selections.
Compatibility simulations cannot discharge P6/P7 native-wire obligations.

## Native subset selected for implementation

IEEE Std 802.15.4-2024, nonbeacon static PAN, 2.4 GHz O-QPSK and 127-octet PSDU.
Data frame version 0b01; extended source; extended unicast destination or short 0xffff
broadcast; same source/destination PAN with PAN compression set. Sequence number present,
security/IEs/sequence suppression absent. Use immediate ACKs for unicast, no ACK for
broadcast. Data MHR is 21 octets for extended destination or 15 for short destination;
with two-octet FCS, adaptation capacities are 104 and 110 octets respectively. These are
native subset budgets, not substitutes for actual legacy frame overhead.

Normative retrieval: `ieee802154-2024:clause:7.2.2.6`, physical PDF page 80,
source locator `ieee802154-2024@367029:368560` (PAN compression);
`ieee802154-2024:clause:7.2.2.10`, page 82 (version);
`ieee802154-2024:clause:7.2.11`, pages 84–85 (FCS);
`ieee802154-2024:clause:7.3.3`, page 91 (ACK format).
Data MHR requirements: `ieee802154-2024:clause:7.3.2.2`, physical pages 90–91.
O-QPSK frame length is the PSDU octet count (§13.1.3.2); modulation/data rate are in
§13.2.1–2. Field-level golden bytes and external agreement remain P6 evidence.
These selections are not a conformance verdict.

Corpus built from the local PDF using `inet_process_standards build` into
`/tmp/lowpan-standards-processed`. Lint has extraction warnings (169 rejected heading
candidates, one tiny node and 18 unresolved references); retrieval is not blanket proof
of resolved cross-references. No source PDF was modified.

## Environment and open evidence

The checkout initially had no INET library. Debug build command:
`make MODE=debug -j8`, log `/tmp/lowpan-build-debug.log`; exit 0 (baseline build).
No existing IEEE 802.15.4 protocol suite was found under `tests/protocol`.
The P1 production receive test is recorded below. Native capture and pinned-peer evidence
are recorded in the final sections; no broad external interoperability claim is made.

## Standards provenance

See [source hashes](source-hashes.sha256). RFC texts are unchanged local copies.
RFC 4944 errata checked at <https://www.rfc-editor.org/errata/rfc4944> on 2026-09-19:
verified 4359 corrects ESC selector range; use the later RFC 6282/8066 dispatch assignments.
Verified 6194 concerns the short-address autoconfiguration wording in §6; it does not
replace RFC 6282 §3.2.2's distinct short-address decompression mapping. Short-address
unicast autoconfiguration is outside the first-release profile.
RFC 6282 errata endpoint returned HTTP 429/unavailable; IETF history synchronized the all-errata-rejected state on 2024-04-24, but a current complete errata check remains open.
Source: <https://datatracker.ietf.org/doc/rfc6282/history/>.
The RFC Editor's indexed [RFC 6282 errata record](https://www.rfc-editor.org/errata/rfc6282)
was retrieved on the same date: record 4814 was rejected 2024-04-23; the original
Hop Limit 255 explanation remains valid. Direct retrieval still returned 429, and
RFC 8025/8066 errata endpoints remained unavailable. This narrows the gap without
claiming a fresh complete database snapshot.

Initial P1 test attempt before the baseline library linked: setup ERROR, missing
`-lINET_dbg` and `-ltest_dbg`; no behavioral verdict. The baseline build subsequently
completed. The focused module runner builds its support library on the next attempt.

## P1 verification

Production change: `Ieee802154Mac::decapsulate()` now copies destination alongside source.
Working directory for build/module commands: checkout root; debug mode, run 0, seed 0.

| Command / artifact | Outcome |
| --- | --- |
| `inet_run_module_tests -m debug -f 'Ieee802154ReceiveMetadata\.test$' --no-concurrent` on original MAC | 1 case FAIL at t=0.001s, event 3: destination assertion; `/tmp/lowpan-p1-before.log` |
| `make MODE=debug -j8` after fix | exit 0; `/tmp/lowpan-p1-build.log` |
| Same module command after fix | 1 case PASS; `/tmp/lowpan-p1-after.log` |
| `./fingerprinttest -d -m 'CSMAWith(UnitDisk|ApskScalar|ApskDimensional)Radio' -f tplx -f '~tNl' -f '~tND' examples.csv` in `tests/fingerprint` | 3 cases PASS, unchanged expectations; `/tmp/lowpan-p1-fingerprint.log` |
| `doc/project/enforcement/check-architecture.sh src/inet/linklayer/ieee802154` | exit 0; `/tmp/lowpan-p1-architecture.log` |
| `doc/project/enforcement/check-seals.sh` | exit 0; `/tmp/lowpan-seals.log` |

The fixture initially had module-class namespace, antenna mobility and interface wiring
setup errors; these were corrected before the baseline destination-assertion failure.
Generated current test artifacts: `tests/module/work/Ieee802154ReceiveMetadata/`.
No fingerprint baseline changed. Release compilation and later feature-off/native gates
are still outstanding. P1 is legacy receive evidence only; P0 and P2–P7 remain open.

## P2a executed foundation and compatibility checks

Debug feature-enabled builds passed through `/tmp/lowpan-boundary-build.log`.
`LowpanNativeAddress.test` passes duplication and parsim round-trip checks, including
64-bit asymmetry and rejected malformed deserialization without mutation. The map test
passes alias collisions and interface-scoped next hops (`/tmp/lowpan-map-unit.log`);
its first run exposed a test namespace/class ambiguity, corrected by qualification.

Module checks passed with `inet_run_module_tests -m debug -f '<explicit names>\\.test$'
--no-concurrent` (logs retain the exact selected names):

- `/tmp/lowpan-domain-module.log`: legacy metadata, configured opaque payload, initial domain.
- `/tmp/lowpan-preparation-module.log`: real-radio domain plus unicast/broadcast preparation
  and unsupported PAN rejection. A fixture-only duplicate fixed NED assignment was corrected.
- `/tmp/lowpan-domain-negative.log`: alias collision, unlisted radio and multiple domains
  on one medium all fail initialization with their expected diagnostics.
- `/tmp/lowpan-boundary-module.log`: actual queue/MAC/radio exchange of 104-octet broadcast
  and unicast payloads, reconstructed native identities/PANs and removed TX/alias tags;
  second parameter-study run uses a zero-capacity queue and observes two queue-overflow
  drops with zero radio transmissions. Both runs pass.

Scoped architecture/naming checks pass for the lowpan package. Independent foundation
review found no correctness defect; its fixture icon flag was repaired and confirmed
clear (21 PASS, 5 N/A, 0 FLAG). That review predates the domain/preparation/boundary modules;
those additions still require their own stable-diff review. Feature-off/release builds,
IPv6 data-plane reachability and later milestones remain open.

Feature-disabled verification passed: `opp_featuretool disable Lowpan` followed by
`make MODE=debug -j8` (`/tmp/lowpan-feature-off-build.log`). The native IEEE 802.15.4
value/tag unit test still passes; four module tests pass with the feature absent
(legacy metadata, opaque payload, unknown selector and mismatched transmit protocol).
Logs: `/tmp/lowpan-feature-off-unit.log`, `/tmp/lowpan-feature-off-module.log`.
The same three CSMA fingerprints pass unchanged (`/tmp/lowpan-feature-off-fingerprint.log`).
Lowpan was then re-enabled for subsequent integration work.

The domain/boundary review found two configuration defects (cross-domain translation,
external-buffer admission) and missing parameter defaults. Corrections and direct negative
fixtures are present; their fresh enabled-build/runtime confirmation is pending. Queued
profile/length/alias invalidation now has a typed drop reason and a three-run test,
also awaiting that build. These are not yet reported as passing checks.

## P2b executed production evidence

Fresh debug library `/tmp/lowpan-p2b-build.log`: PASS. Native request/header retry
consistency and domain/queue restrictions: ten P2a module cases PASS in
`/tmp/lowpan-review-fixes-module.log`; independent review cleared both remaining findings.
Address/map/uncompressed dispatch unit tests: three PASS in `/tmp/lowpan-dispatch-unit.log`.

Seven module cases PASS in `/tmp/lowpan-p2b-module.log`: real IPv6 echo and oversized
single-frame rejection, UDP unicast/multicast, orderly stop/start and crash/restart,
mixed Ethernet/native ND, malformed receive rejection/recovery, retained retry mismatch,
and queued-envelope invalidation. The lifecycle fixture exposed and now verifies receiver
mode restoration in the opt-in compatibility MAC. Native address readiness is immediate;
ordinary Ethernet DAD still runs. No default legacy MAC lifecycle behavior was changed.

Standalone example and network test: `inet --debug -u Cmdenv -f omnetpp.ini -c General -r 0`
from `examples/lowpan` and `tests/networks/lowpan`, respectively. Logs
`/tmp/lowpan-example.log` and `/tmp/lowpan-network-test.log`: one request and one reply,
zero loss each. Both are explicitly compatibility-only; current oversize behavior remains
not release-capable. Fragmentation, compression and external native-wire validation remain
NOT_RUN. Source-fragmentation/routed next-hop/queued POST_ROUTING coverage is still owed
before P5. Release-mode and the final feature matrix remain open.


## P3 executed fragmentation and lifetime evidence

`/tmp/lowpan-p3-unit.log`: four units PASS (fragment golden bytes/planning,
reassembly/region tags, all65536 tag allocations/wrap guard, uncompressed tooling).
`/tmp/lowpan-p3-module.log`: five modules PASS, including byte-exact1280-octet echo
(28 MAC data frames), queued/retry deadline cancellation and slow propagation.
`/tmp/lowpan-p3-lifecycle.log`: fragmented crash/restart and duplicate-insensitive
reassembly expiration PASS. `/tmp/lowpan-p3-queue-refusal.log`: bounded queue refusal
leaves no partial upper delivery. `/tmp/lowpan-p3-profile-rejection.log`: moving
radios and delayed radio input paths rejected. Radio-input validation was subsequently
corrected to permit only the built-in, zero-delay cIdealChannel used for display metadata;
positive1280 exchange plus delayed-path rejection pass in `/tmp/lowpan-p4-module.log`.

The planner review found and corrected rejection of a legal short final fragment.
Production reordered/interleaved fragment traffic and exact one-tick lower-deadline
admission remain additional acceptance evidence; helper coverage does not replace them.

## P4/P5 executed compression and route-over evidence

`/tmp/lowpan-p4-unit.log`: independent IPHC TF/address/CID vectors and truncation PASS.
`/tmp/lowpan-p4-module.log`: IPHC1280 echo uses26 MAC data frames, restores every payload
byte, and carries the expected literal compressed header. Uncompressed1280 still passes.
`/tmp/lowpan-p5-unit.log`: IPHC and UDP NHC golden vectors PASS, including four port modes,
range boundaries, length inference, carried checksum and C=1 rejection.
`/tmp/lowpan-p5-module.log`: UDP NHC production unicast/multicast and single/fragmented
cases PASS; IPHC echo PASS. That log's route fixture failure is superseded by
`/tmp/lowpan-p5-route.log`, which passes both1280 and1281 cases after fixing the fixture's
node-level forwarding parameter. The initial two-NIC test also exposed a real shared
service-registration conflict; transmit dispatch now uses the existing InterfaceReq
registration for each NIC, and the per-NIC LoWPAN service registration was removed.

Route-over checks native next hops/PANs on each side, complete router ingress before
fragmented egress, global IPv6 addresses, Hop Limit32→31, and source IPv6 fragments
of1280/57 octets for a1281-octet local datagram. Queued POST_ROUTING continuation,
two distinguishable flows and full negative compression matrices remain open.
Independent static reviews found no P4/P5 codec or original-coordinate defect; the P5
wire-length duplication finding was corrected with one shared helper. Final build/gates
and native external validation are still required.

## P6 native implementation and independent validation

Native data/ACK frames use the pinned version1 extended-address/short-broadcast
profile, PAN compression, little-endian fields and computed two-octet FCS. The
standalone examples now use native domains and interfaces. The route example
(`inet --debug -u Cmdenv -f route.ini -c General -r 0` in `examples/lowpan`) records
one UDP sink delivery across two independently configured PANs; single-hop
`omnetpp.ini` records one request/reply of1280-byte IPv6 datagrams.

Executed debug evidence:

- `/tmp/lowpan-final-module.log`: 48 focused module fixtures PASS using
  `inet_run_module_tests -m debug -f '(Lowpan.*|Ieee802154ReceiveMetadata|Icmpv6LocalChecksum|Icmpv6ExtensionChecksum|ICMPv6_delivery|IPv6_packet_too_big|MLD_smoke)\.test$' --no-concurrent`.
- `/tmp/lowpan-final-unit.log`: 10 focused units PASS using
  `inet_run_unit_tests -m debug -f '(Lowpan.*|Icmpv6PseudoHeader)\.test$' --no-concurrent`.
- `/tmp/lowpan-final-extra.log`: `LowpanNativeTwoFlows` and
  `LowpanNativeDeadlineBoundary` PASS. The latter checks a maximum127-byte broadcast
  and deadlines differing by one simulation tick; one drops and one transmits.
- Native lifecycle, retry, lifetime, malformed byte-only receive, UDP unicast/broadcast,
  ICMP1280, source fragmentation and forwarded PTB are included. Queued POST_ROUTING
  route-over passes1280/1281; the fixture queues after checksum insertion because the
  existing IPv6 reinjection API resumes after the hook stage, not the remaining hooks.

Commands run from the checkout root with `MPLCONFIGDIR=/tmp/lowpan-matplotlib`;
fixtures pin run/seed and any iteration matrix. Logs under `/tmp` are local transcripts;
the named fixtures and scripts are durable reproductions, not claims based on log names.
These results precede the final signal-observable consistency change and the additional
CCA/retry-exhaustion/accept-matrix fixtures; their fresh results are recorded below.

Independent TShark4.6.4 decodes LINKTYPE_IEEE802_15_4 captures with FCS included:
[check-capture.py](../../protocol/6lowpan/checks/check-capture.py) checks every frame
and each reconstructed ICMPv6 checksum; [native-capture.tsv](native-capture.tsv)
records hashes/counts. All108 frames have valid FCS and fit127 bytes; all four
reconstructed echo messages have valid checksums, including source fragmentation.

The four `LowpanNativeUdp` runs additionally record source data frames for small
and fragmented UDP, both extended unicast and short broadcast. The same checker
enables TShark UDP checksum validation. All 27 frames have valid FCS and fit the
127-byte PSDU limit; all four reconstructed UDP checksums are valid. See
[native-udp-capture.tsv](native-udp-capture.tsv). Reproduce with the module fixture
and pass its `tests/module/work/LowpanNativeUdp/native-udp-{0,1,2,3}.pcapng` files
to the checker. This independently decodes the native EUI and multicast forms;
it does not turn the separate ns-3 elided-address mismatch into a passing exchange.

The [pinned ns-3 exchange](../../protocol/6lowpan/checks/peer/README.md) passes in
both directions for fragmented global-address UDP with carried checksum. Reverse,
interleaved peer fragments enter the production adaptation receive path and match
every original IPv6 byte. This caught a raw-byte representation issue hidden by
local chunk round trips; consuming/trimming compressed headers avoids a defective
nonzero-offset erase branch in the sealed packet core. No sealed file was modified.
Native-EUI fully elided addresses and broadcast remain outside this peer evidence.

Independent inspection cleared the production changes. Scoped architecture and naming
gates pass for lowpan/ieee802154; ICMPv6 naming reports two unchanged candidates.
`check-source-seals.sh --diff` passes for tracked and untracked source changes.
The global `check-interfaces.sh` reports15 existing violations outside this change;
none names the new LoWPAN contracts. No exception or baseline was added for them.

Both debug and release library builds pass. The release library was rebuilt after
byte-normalization (`/tmp/lowpan-final-release-build.log`). Final feature-off and
post-observable-change results follow; the complete release claim remains open.

### Release limits still open

The implementation remains in `plan/pending/`. P0's complete current errata retrieval
is not closed: the official RFC4944 list confirms verified4359 and6194, but the
RFC6282 complete-list endpoint still returns429; the indexed rejected4814 record
is not a substitute for a current full list. The RFC Editor's new RFC6282 info page
links to `https://errata.rfc-editor.org/search/?rfc_number=6282`; that endpoint and
the corresponding RFC8025/RFC8066 searches were also inaccessible on 2026-09-19.
The controlled-budget fixture now exercises placement fallback separately from the
native profile, whose minimum 104-byte budget exceeds its supported compressed
IPv6/UDP header chain. Dynamic ND, association/disassociation, security, short
unicast and deferred P8–P15 features remain outside this implementation.

The external peer limitation is reproduced in the saved elided-address vectors:
ns-3 reconstructs both IID U/L bits differently. It must remain a visible failed
interoperability case, not be normalized away or reported as full RFC6282 support.

Feature-disabled final build: `opp_featuretool disable LowpanExamples LowpanTests Lowpan`
then `make MODE=debug -j12`, exit0 (`/tmp/lowpan-final-feature-off-build.log`). Five
legacy/shared module fixtures PASS (`Ieee802154ReceiveMetadata`, `ICMPv6_delivery`,
`IPv6_packet_too_big`, `Icmpv6LocalChecksum`, `Icmpv6ExtensionChecksum`) in
`/tmp/lowpan-final-feature-off-module.log`. The three selected existing802.15.4
fingerprints PASS unchanged in `/tmp/lowpan-final-feature-off-fingerprint.log`, using
the P1 fingerprint command. No baseline or ordinary legacy wire format changed.

Core without UDP: enable only `Lowpan` from the feature-off state, then
`opp_featuretool disable -f Udp` and `make MODE=debug -j12`.
`/tmp/lowpan-core-without-udp-build.log`: exit0. All11 selected units PASS in
`/tmp/lowpan-core-without-udp-unit.log`, including the added legal nonminimal
stateless unicast/CID accept matrix and every header truncation. Two production
modules PASS in `/tmp/lowpan-core-without-udp-module.log`:1,280 literal IPHC receive
combinations and a busy PHY-state pulse that ends before CCA sampling completes.
The loaded library exports LowpanUdpNhcCodec but no Udp transport implementation.
The original feature state is restored afterward, with Lowpan/Examples/Tests enabled.

### Final expanded verification

The restored debug and release builds both pass (`make MODE=debug -j12` and
`make MODE=release -j12`; `/tmp/lowpan-restored-{debug,release}-build.log`). All
11 focused units pass after the final production changes:

```
MPLCONFIGDIR=/tmp/lowpan-matplotlib inet_run_unit_tests -m debug -f '(Lowpan.*|Icmpv6PseudoHeader)\.test$' --no-concurrent
MPLCONFIGDIR=/tmp/lowpan-matplotlib inet_run_module_tests -m debug -f '(Lowpan.*|Ieee802154ReceiveMetadata|Icmpv6LocalChecksum|Icmpv6ExtensionChecksum|ICMPv6_delivery|IPv6_packet_too_big|MLD_smoke)\.test$' --no-concurrent
```

The module campaign selected 65 fixtures: initially 62 PASS and three fixture
failures. `LowpanCompressionBudget` needed to declare its interface-table parameter;
`LowpanNativeTurnaroundLifecycle` needed a zero PingApp stop timeout so shutdown
reaches the MAC during turnaround; `LowpanNativeDomainSharedMedium` expected the
wrong rejection diagnostic. No production change was required. The corrected
three fixtures and the UDP fixture with capture recording all PASS on this rerun:

```
MPLCONFIGDIR=/tmp/lowpan-matplotlib inet_run_module_tests -m debug -f '(LowpanCompressionBudget|LowpanNativeTurnaroundLifecycle|LowpanNativeDomainSharedMedium|LowpanNativeUdp)\.test$' --no-concurrent
```

Thus all 65 selected fixtures have passing final results; this is the initial
campaign plus the focused correction run, not a claim of a second full campaign.
Transcripts: `/tmp/lowpan-expanded-{module,unit,recheck}.log`.

The additions cover 1,280 independently assembled IPHC receive combinations;
16/44/50-byte lower budgets selecting uncompressed dispatch, inline UDP and NHC;
native retry exhaustion and wrong ACK sequence; envelope changes before first TX
and retry; crash/shutdown during turnaround; exact deadline boundaries; native
domain rejection cases; uncompressed native exchange; and two routed UDP flows.
The CCA pulse fixture injects PHY state transitions, not a physical interferer.

All three documented example commands complete successfully with computed wire
checksums (single-hop IPHC, uncompressed, and two-PAN route-over). Fresh route-over
vectors exactly match the pinned peer artifacts; `checks/peer/run.sh` passes again.
Scoped architecture/naming and tracked/untracked source-seal checks pass after the
last production change. `git diff --check` passes. No fingerprints were updated.
