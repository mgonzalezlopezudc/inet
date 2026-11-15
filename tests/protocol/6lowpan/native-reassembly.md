# Native reassembly protocol coverage

> **Kind:** report · **Status:** snapshot 2026-11-15 · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-implementation-plan.md](../../../plan/done/6lowpan-implementation-plan.md), [data-plane.md](../../../doc/project/evidence/protocol/6lowpan/checks/data-plane.md)

P7 needs evidence that the native MAC's decoded identity and byte-only payload actually reach the production RFC 4944 reassembly and RFC 6282 decompression paths, and that native transmission honors fragment boundaries. Helper tests establish the algorithms' rules but cannot show that integration. This change adds coverage; it does not repair a demonstrated production defect or change the supported release profile. Its proposed subject is `test: add: native 6LoWPAN boundary protocol coverage`.

The companion module fixture `LowpanIphcReceiveMatrix.test` now supplies its 1,280 literal encodings as complete native data-frame bytes rather than creating native indication tags at the adaptation input. Its expected IPv6 byte arrays are unchanged. The MAC must reconstruct the source, extended-unicast or short-broadcast destination, and PAN metadata before the adaptation codec can satisfy those expectations. This closes the native-boundary gap for the existing TF/HLIM/address/CID acceptance matrix. It remains receive-only evidence and does not test compressor selection for every encoding.

## Setup and observation

`LowpanNativeReassembly.test` creates two stationary native interfaces in PAN `0x1234` on a dedicated IEEE 802.15.4 scalar medium. The observer injects independently constructed data-frame bytes, including address fields and computed FCS, at node 0's native MAC receive gate. The MAC decodes and filters the frame, creates native indications, and passes the payload through the production boundary and adaptation module. The observer receives the resulting IPv6 bytes at the interface output. No sender chunk objects or request tags accompany the input. This isolates receive behavior; it does not test propagation, channel access, or sender ACK/retry behavior.

The fixture uses seed 0 and a 160 ms simulation. Run 0 permits two contexts and 240 bytes, so a third 80-byte datagram hits the context limit. Run 1 permits three contexts and 160 bytes, so the same stimulus independently hits the byte limit. All other stimuli and assertions are identical. Each run injects 61 frames and requires exactly nine byte-exact IPv6 deliveries at named times, eight completed reassemblies, one resource refusal, four reassembly rejections, 30 malformed-input drops and two unsupported-dispatch drops. Every injection is counted, so an absent error cannot pass because its stimulus was skipped.

## Protocol cases

| Stimulus | Required observation and reason |
| --- | --- |
| FRAGN arrives first for interleaved tags 65535 and 0; one FRAGN repeats | Exactly one complete datagram per tag, in completion order. RFC 4944 section 5.3 permits out-of-order arrival; an exact duplicate supplies no new coverage. This tests receiving these tag values, not sender-side tag allocation or guard expiry. |
| A 48-byte first range is followed by a range covering bytes 40–79 | The overlap discards the old partial state. The implementation's documented restart policy keeps the incoming range; a new nonoverlapping first range completes the original bytes. |
| A same-range first fragment changes Hop Limit; a final fragment follows | No delivery. The documented local hardening policy quarantines conflicting bytes through the original 30 ms expiry. Quarantine is not presented as an RFC requirement. |
| A final range arrives at 40 ms and repeats at 49 ms | Exactly one expiry at 50 ms. A new first fragment at 51 ms cannot combine with the expired range and expires at 61 ms. Duplicates do not extend the first-arrival deadline. |
| A third partial datagram arrives while the configured resource limit is full | One refusal, followed by successful admission and completion after another datagram frees its reservation. |
| A non-final range has 41 bytes; another range extends past the datagram size | Both reject without partial delivery. Subsequent valid fragments with that tag deliver exactly once. |
| Literal `7a 33 3b` plus eight payload bytes | The independent 48-byte IPv6 expectation has both EUI-derived IID U/L bits inverted, as required by RFC 6282 section 3.2.2. This directly distinguishes the stored peer disagreement. |
| Truncated IPHC `7f`, unknown dispatch `42`, empty payload, page switch `50`, reserved stateful destination | Three malformed and two unsupported outcomes, with no extra IPv6 delivery. Recognizing an IPHC dispatch does not make a truncated encoding valid. |
| Truncated FRAG1 and FRAGN without range bytes | Two malformed outcomes; neither can create a deliverable datagram. |
| Same tag and link identities, but an additional last range declares size 72 while the original declares 80 | The size-72 context cannot complete the size-80 datagram. The valid size-80 last range completes exactly the original bytes at 102 ms; the independent size-72 context expires at 111 ms. Datagram size is part of the RFC reassembly identity, so this does not require dropping every different-size fragment immediately. |
| Missing NHC, every nonempty proper truncation of each of the four UDP NHC header forms, all four C=1 forms, and an unsupported NHC identifier | All 25 inputs reject as malformed with no extra IPv6 delivery. Checksums cannot be elided without qualifying integrity support. |

## Execution and limits

`LowpanNativeFragmentBoundaries.test` complements the injected receive cases with real sender/radio delivery. A test driver supplies one independently constructed IPv6 datagram at a time to a native interface. The sender's MAC output is checked as bytes: extended-unicast or short-broadcast destination and PAN fields, PSDU at most 127 bytes, RFC datagram size and tag fields, consecutive original-coordinate FRAGN offsets, eight-byte non-final coverage, literal uncompressed or independently predicted IPHC prefix, and exact remaining payload bytes. The remote native interface must deliver the original IPv6 bytes before the driver advances to the next size. A drop fails immediately. Every size 40–1280 is tested, so all capacity and alignment edges are included. Four runs cover unicast/broadcast crossed with uncompressed/stateless IPHC: 1,241 exact deliveries per run, 4,964 total. All use seed 0 and a 300 s upper simulation limit. The inline Next Header is 59; UDP/NHC behavior is covered separately by existing native UDP fixtures and captures. This is a finite deterministic boundary campaign, not a loss/retry probability measurement.

Run from the INET root with a matching debug library:

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'LowpanNative(Reassembly|FragmentBoundaries)\.test$'
```

The [release validation record](../../../doc/project/evidence/model/6lowpan/release-validation.md) owns the executed verdict, source and fixture hashes, and logs. Both fixtures pass on the existing production source. No failing-parent claim is made. The initial setup attempt failed to resolve the observer's NED namespace. One intermediate different-size input used 81 bytes, inadvertently exceeding the 160-byte resource budget with the active 80-byte context in run 1; using 72 isolates identity behavior from resource refusal. Both fixture corrections and their raw failures are retained; neither required a production change or an expected-failure declaration.

The receive fixture does not establish radio delivery; the sender fixture supplies that evidence for its selected two-node topology. Neither establishes sender tag wrap/guard exhaustion, cross-interface/PAN identity isolation, lifecycle teardown, or external peer agreement. Those observations are supplied separately by the [identity](native-identity.md), [receive lifecycle](native-receive-restart.md), [tag guard](native-tag-guards.md), capture and peer checks in the first-release validation record.
