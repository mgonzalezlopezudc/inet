# Native sender tag guards

> **Kind:** report · **Status:** snapshot 2026-11-15 · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-implementation-plan.md](../../../plan/done/6lowpan-implementation-plan.md), [data-plane.md](../../../doc/project/evidence/protocol/6lowpan/checks/data-plane.md)

The P7 sender row needs the real adaptation sender, not only an allocator helper. `LowpanNativeTagGuards.test` drives that path through an entire 16-bit tag cycle, restart, and the exact reuse deadline. Its planned commit is `tests: add: native 6LoWPAN sender tag guard coverage`. It adds a control for existing production behavior and makes no bug-repair claim.

Two stationary native interfaces supply the real static-domain and native-MAC preparation contract. The driver submits valid 104-byte IPv6 datagrams to the first interface with uncompressed dispatch. Its actual 104-byte native payload budget makes each datagram require two fragments. The production `LowpanLayer` allocates tags, encodes the headers, and pushes them through the native boundary. The observer reads the serialized fragment bytes and checks tags 0 through 65535 in order, original size 104, offsets 0/96 and exact header lengths. It also checks the real native preparation profile, capacity and one-second lower-delivery deadline.

A test subclass of `PacketQueue` accepts and discards each frame at the queue input, before MAC transmission. It retains the concrete queue type and bounded parameters required by the native boundary. This models controlled loss without accumulating packets or transmitting 131,076 frames. It does not replace the sender, allocator, boundary or MAC's preparation computation. Every discarded frame is counted, and any MAC transmission fails the test. This is production sender/guard integration evidence, not radio delivery or ordinary queue-overflow evidence. The separate sender boundary sweep exercises the full queue/MAC/radio path.

At 1 ms the driver makes 65,537 attempts: the first 65,536 allocate every tag once, and the next refuses guarded tag 0 without skipping it. Run 0 shuts the node down at 2 ms; run 1 crashes it. Both start at 3 ms and independently prove the node was DOWN at 2.5 ms. A 4 ms attempt must still refuse tag 0. The first preparation deadline is 1.001 s; the RFC receiver-lifetime guard extends reuse to 61.001 s. A probe one simulation tick before that deadline refuses; the exact-deadline and one-tick-after probes allocate 0 and 1 respectively.

Each run requires 65,541 attempts, 65,538 accepted datagrams/allocations, 131,076 observed and discarded fragments, and exactly three tag refusals at the specified times. These counts and direct serialized-tag observations prove wrap, full exhaustion, non-skipping refusal, stop/crash retention and exact reuse. No private allocator state is inspected or modified. Seed 0 and the two finite lifecycle variants are sufficient for this deterministic claim. The 62 s simulation limit does not imply a 62 s wall-clock stress test.

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'LowpanNativeTagGuards\.test$'
```

Exact source/fixture hashes and executed results are in the [closure record](../../../doc/project/evidence/model/6lowpan/p7-closure.json). The parent already passes; production source, wire semantics and baselines are unchanged. Sender guards mitigate reuse under the bounded stale-fragment lifetime assumption; they do not solve wire-identical nonconflicting generations.
