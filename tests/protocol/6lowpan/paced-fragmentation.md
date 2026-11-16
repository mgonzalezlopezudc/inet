# Paced native source fragmentation

`LowpanPacedFragmentation.test` exercises the opt-in source scheduler through
production adaptation, native boundaries, bounded queues, native MACs and actual
radios. Configuration declarations live in
[`LowpanLayer.ned`](../../../src/inet/linklayer/lowpan/LowpanLayer.ned).
The helper owns admitted originals, immutable encoding/ranges and native peer
tuples. Each flow has one lower frame outstanding. A matched ACK permits the next
frame after the configured gap; the admission deadline never moves. Source count
and retained-original bytes are separately bounded. Encoded headers fit one native
frame and range vectors have at most 256 entries per flow, providing additional
fixed metadata bounds. Tags remain guarded after cancellation and ordinary restart.

```mermaid
flowchart LR
    S[Scripted IPv6 source] --> A[Real source adaptation / boundary / queue / MAC]
    A -->|Native data over radio| B[Real destination MAC / boundary / adaptation]
    B -->|Actual immediate ACK| A
    B --> C[Byte comparison]
```

The two stationary nodes are in mutual radio range. The link domain configures
their actual extended identities and a source next-hop binding. There is no
automatic host registration or multihop forwarding in this fixture. The checker
injects complete byte-valued IPv6 datagrams at the source interface, observes
fragment submission and native terminal results, and compares every delivered
datagram byte for byte. Most ordinary payload uses Next Header 59. Run 26 adds
independently constructed UDP bytes with a valid IPv6 pseudoheader checksum,
without a UDP transport module or application dependency.

| Runs | Stimulus and required observation |
| --- | --- |
| 0 | A 1280-byte datagram crosses the actual radio. Every later fragment follows the preceding matched ACK by at least 500 ms; all bytes reassemble. |
| 1 | Receiver radio is off. Two FRAG1 attempts exhaust the native retry budget; no FRAGN is submitted and the retained original is aborted. |
| 2 | The receiver gets FRAG1 and sends real ACKs. A fixture-only MAC subclass discards those ACKs at the source lower entry. Two attempts, two observed ACK drops and no FRAGN prove that receiver reception alone does not release the barrier. |
| 3–4 | Two same-event datagrams compete for one count slot or insufficient original-byte capacity. Exactly one is admitted and delivered; the other gets a resource refusal. |
| 5 | Admission at 0.1 s has an 0.8 s residence. Two frames succeed; the flow expires at exactly 0.9 s, and its third frame is never submitted. Each prepared deadline is clipped to 0.9 s. |
| 6–7 | Whole-node STOP or CRASH between frames clears the old original. Restart sends only the newly admitted datagram, using a different tag. |
| 8 | Both peers have CID 5 expiring at 0.2 s, after accepted FRAG1. Remaining opaque tails finish with the frozen interpretation and deliver the original global addresses. |
| 9,12–16,25 | NS, RA, NA, DAR, DAC, Redirect and RS are classified as control and retain immediate fragmented emission. Redirect includes a Hop-by-Hop extension. These are deliberately synthetic control bodies: their wire classification/timing is tested, not their ND semantic acceptance. |
| 10–11 | Broadcast and default-off ordinary unicast retain immediate emission. |
| 17 | A 2047-byte datagram exercises the maximum RFC 4944 size with paced byte-exact delivery. |
| 18 | A zero-capacity source queue refuses the first frame. No radio transmission occurs; retained ownership is released once. |
| 19 | A fragment observer stops the whole source before lower push. The removed flow cannot be used after the observation returns; restart admits a fresh tag and datagram. |
| 20–21 | MAC-only STOP or CRASH while FRAG1 is outstanding cancels the flow even though Layer remains up. With count one and bytes exactly one datagram, restart admits fresh work well before the old deadline. |
| 22 | A native-success observer stops only the MAC. The copied frame identity permits cleanup after that observation; the old flow cannot resume. |
| 23–24 | MAC-only STOP or CRASH occurs during the ACK-to-next-send pause, after exactly one observed success. The interface-table DOWN notification clears waiting ownership even though there is no outstanding MAC original to report failure. Fresh admission after restart proves capacity release. |
| 26 | A 1280-byte IPv6/UDP datagram uses compressed UDP ports and a valid independently constructed checksum. Actual radio FRAG1 has the literal UDP NHC encoding; its original header is 48 bytes, its payload starts at original byte 48, and the first FRAGN starts at original offset 104. Fourteen ACK-paced radio transmissions deliver identical bytes and preserve the valid checksum. |
| 27 | The first admission listener stops the source and then reads the admission payload; a second listener reads it afterward. A fixture lifetime tracker checks liveness before either dereference. The helper's local emission snapshot remains alive even though the cancelled flow's original is deleted. No cancelled source frame is emitted. Restart delivers only fresh tag 1; both listeners read twice and all tracked packets are destroyed. |

The MAC subtype in the fixture uses the production native implementation except
for run 2's explicit ACK filter. Its NED lifecycle property permits public
MAC-only operations; the production MAC is unchanged. Whole-node operations use
the normal lifecycle controller. No test mutates private source-flow state.

The production discovery regression fixture remains in the mapped selection. Ordinary native
completion, deadline, retry, queue and lifecycle fixtures remain mode-off controls.
Mesh plus source pacing is explicitly rejected at initialization. Compatibility
interfaces do not acquire this native scheduling capability.

The [verification record](../../../doc/project/evidence/model/6lowpan/p14-paced-source-verification.json)
publishes exact commands, matching libraries and output hashes. All 28 final cases
pass in debug and release. The final debug selection contains 20 protocol fixtures and 147
simulations; release contains six protocol fixtures and 73 simulations. Both modes
are rebuilt and the mapped controls rerun after the admission-snapshot repair.
Prior 26- and 27-case inputs, outputs and records are retained separately. Both modes
also pass 14 mapped module fixtures (38 completed simulations and one expected
initialization diagnostic). The 28 pacing, 43 sibling-protocol and 20 native
completion counter rows match independently between modes. Debug assertions establish spacing, byte coverage and
terminal ownership. Release assertions are disabled: only normal completion and
independently compared printed counters are claimed for release. Pacing is not
virtual reassembly forwarding, selective recovery, or destination completion
acknowledgement. Exhausting all source tags is not repeated here; the existing
bounded allocator is unchanged, while this caller supplies the configured guard.

Separate initialization controls reject Mesh composition, insufficient tag
protection and a sub-tick gap with their specific diagnostics. An exact minimum
protection configuration succeeds. A supplementary normal-epoch femtosecond-clock
run accepts raw durations beyond the representable clock range by saturating before
conversion, then exercises actual first-frame retry failure and cleanup. It is not
a large-epoch PHY-precision test. These five invocations are recorded separately
from the normal suite counts.

Preserved attempts distinguish two fixture lifecycle-API compile errors, the
initial production include-name compile error, and the external-subclass compile
error fixed by defining the Layer constructor out of line. The first 23-case pass
precedes the interface-down gap repair. An intermediate assertion expected a
failure-drop observation after an observer stopped the MAC; the synchronous DOWN
notification correctly cancels ownership first, so that fixture expectation was
corrected. The latest matching runs above supersede those attempts.
