# Stateful IPHC contexts

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-completion.md](../../../plan/done/6lowpan-publication-plan.md), [RFC 6282 catalog](../../../doc/project/evidence/standard/rfc6282/catalog.md)

Publication association: [C08](../../../plan/done/6lowpan-publication-plan.md#c08); donor `58b446708d`. The retained setup, assertions and parent explanation below describe this behavior. Original run counts, PASS statements and temporary artifact locations refer to the donor cohort. New-prefix results are recorded separately in the publication verification manifest; none is inferred from those historical statements. Exact-parent runtime execution is not claimed unless that manifest records it.


P8 adds RFC 6282 sections 3.1.1–3.2.4 context-based address compression. The parent codec explicitly rejects stateful source/destination forms and has no context owner. The adaptation module now owns a bounded table of canonical prefixes and validity deadlines. Compression permission is independent from decompression eligibility: a retiring context may still decode in-flight input. Empty tables retain the existing stateless behavior. Context ID 0 is implicit; nonzero and unequal source/destination IDs use the extension octet.

`LowpanStatefulContexts.test` uses two native interfaces in PAN 0x1234. Literal data frames enter the real MAC with independently constructed IPHC bytes and FCS. The adaptation output must equal independently assembled IPv6 bytes. Context 3 selects source prefix 2001:db8:1::/64, context 5 selects destination prefix 2001:db8:2::/64, and context 0 supplies an implicit common prefix. Receive-only contexts decode successfully. Unknown context 9 and input at the exact source-context expiry time reject without delivery.

A compressed first fragment at 8 ms is decoded while context 3 remains valid until 8.5 ms. Its tail at 9 ms completes the original 80-byte packet after expiry, proving that already reconstructed address bytes are a stable snapshot. A separate FRAGN-first context starts at 11 ms. Prefix replacement at 12 ms must invalidate that partial state and emit one invalidation. The new first fragment at 13 ms cannot complete using the old tail; a new tail permits delivery at 14 ms. This tests the actual owner mutation path rather than modifying helper state behind the module.

The same receive path handles independently encoded stateful multicast. Finally, a real IPv6 input crosses the sender, native boundary, queue, MAC and radio. Context 5 is valid but forbidden for compression, so the emitted source address stays inline while destination context 3 is selected. The fixture checks the exact IPHC prefix, CID, inline source bytes and delivered IPv6 packet. Each seed-0 run requires ten injected native frames, six byte-exact deliveries, two malformed-context drops, one partial-context invalidation, two completed reassemblies, five observed fragments and one real data transmission. It ends at 50 ms.

`tests/unit/LowpanStatefulContexts.test` supplements this with independent serialized vectors for source/destination modes 1/2/3, CID 0 and unequal IDs, stateful multicast, 32/64/65/128-bit prefixes, prefix precedence over overlapping IID bits, missing/expired contexts, receive-only permissions and truncated headers. A 128-bit context can reconstruct a fully known address without an encapsulating IID. Invalid identifiers and noncanonical prefixes reject as configuration errors.

```sh
opp_run_unit_tests --load . -p inet -m debug --no-build -f 'LowpanStatefulContexts\.test$'
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'LowpanStatefulContexts\.test$'
```

The broader current unit/module/native protocol selections and matching builds are recorded in `/tmp/lowpan-p8/verification.json`; the completion evidence tracks actual verdicts. A parent runtime comparison using this complete fixture is not claimed: its runtime context-install API is absent on the parent. Source inspection establishes the previous fixed rejection; build/setup incompatibility is not presented as a behavioral failure. The initial new unit attempt used an incorrect enum spelling and supplied no execution evidence until corrected.

This package implements static configuration and the context-owner update operation. Context advertisement/distribution and optimized ND belong to P10. Prefix replacement conservatively invalidates all partial datagrams on that interface. Same-prefix permission/validity refresh does not reinterpret stored bytes. Security and authorization of advertised contexts remain separate from codec correctness.
