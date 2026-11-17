# Core selective-recovery path

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [native recovery report](selective-recovery.md)

Publication association: [C40](../../../plan/done/6lowpan-publication-plan.md#c40); donor `66192266b8`. The retained setup, assertions and parent explanation below describe this behavior. Original run counts, PASS statements and temporary artifact locations refer to the donor cohort. New-prefix results are recorded separately in the publication verification manifest; none is inferred from those historical statements. Exact-parent runtime execution is not claimed unless that manifest records it.


`LowpanSelectiveRecoveryNoUdp.test` uses three ordinary IPv6 hosts with UDP and
TCP disabled, two separate native PANs and a two-interface router. It supplies
raw IPv6 at the public native interface input, installs real IPv6 routes and
configured native link-local next-hop mappings, then observes actual native
MAC/radio traffic and byte-exact adaptation output. It has no UDP header,
application or transport dependency. The selected configuration is defined by
[LowpanLayer.ned](../../../src/inet/linklayer/lowpan/LowpanLayer.ned).

```mermaid
flowchart LR
    A["raw IPv6 source"] -->|"native PAN1"| B["route-over router"]
    B -->|"native PAN2"| C["normal IPv6 receive path"]
```

The normal320-byte source datagram has five frozen source ranges. Receiver
output must equal all source bytes with only the outer Hop Limit decremented.
Creation-time region tags are checked over the unchanged payload ranges through
source fragmentation, relay header translation and receiver reconstruction.
Overlap and duplicate cases attach deliberately different metadata to later
copies: already accepted bytes retain their original provenance, while only
newly covered bytes acquire new tags. This is an internal packet-metadata check;
wire capture cannot establish it.

| Case | Production-path check |
| --- | --- |
|0–3 | Raw Next Header59, loss repair, a complete padding HBH header and a nested IPv6 raw tail recover byte-for-byte. |
|4–5 | A processing-bearing option or a chain incomplete in the first fragment causes explicit transit NULL and no early forwarding. |
|6 | X reset arrives while a relay lower original is outstanding. Retained data is released immediately, a fresh identity fits the260-byte bound, and the saved NULL-only reverse correlation still works. |
|7–8 | Marked receipt is echoed once in the next ACK; a new marked duplicate at a FULL record is echoed again without duplicate delivery. |
|9–10 | Consistent overlap and different-metadata duplicate preserve first-byte provenance; inconsistent bytes abort. |
|11 | Same-key first fragment changes D: no reinterpretation/restart. |
|12–13 | Receiver record/data capacity refuses another identity without evicting the admitted one. |
|14 | Burst input fills the bounded relay pending queue; transient drops/deadlines remain repairable by later valid copies. |
|15 | A queued sequence changes shape before submission. It is rejected immediately, data charges are released and a fresh identity succeeds. |
|16–20 | Unknown nonfirst reset, unknown empty first reset with/without X, stateless header-bearing first reset and known-path nonfirst X reset follow their distinct NULL/correlation rules. |
|21–22 | FULL becomes abort protection; repeated input cannot renew first-seen history. A later same-key first fragment is accepted after that fixed history ends. |
|23–24 | A hook added after first output cancels continuation; a hook present initially prevents output. |
|25 | Default-off recovery follows ordinary reassembly and IPv6 forwarding. |
|26 | STOP/START the ingress owner after an X reset. Its NULL-only reverse mapping is removed; a late physical NULL cannot regain that path. |
|27–29 | Hop Limit65→64 shrinks the header. A nonfirst range inside compressed H is refused before its translated offset could become reset offset0; overlap beginning at H remains valid. |
|30–32 | An unknown nonfirst IPv6 fragment and all seven ND/control types, including complete extension chains, retain the ordinary source path. A subsequent eligible datagram still fits the one-record recovery quota. |
|33 | Local receiver recovers a valid1400-byte IPv6 datagram; transit MTU checks do not globally truncate16-bit wire sizes. |
|34 | A wire-valid first fragment exceeds only the actual egress adaptation cap. Its smaller retries cannot revive the protected identity or extend its history; a later fresh admission succeeds. |
|35 | Simultaneous local source and ingress relay share one egress8-bit pool. Both distinct native tag identities and both metadata-preserved datagrams recover correctly. |
|36 | Delete the ingress Layer while its swapped tag remains guarded. Stateless resets reserve the other255 tags; after166s the sole freed byte is reused by the surviving egress Layer's source. Its real FULL must complete the new owner without a timeout probe. |

Literal recovery inputs use the real native boundary/MAC/radio, with explicit
wire fields. They bypass the compliant source allocator/partition only for the
adverse identity/shape/history controls. In particular, the post-history reuse
tests do not claim that a compliant source reuses its tag before its longer
guard expires. Header construction uses the existing stateless codec; the
independent [wire unit](../../unit/LowpanRecoveryWire.test) supplies literal
bit-field/truncation vectors. Header sizes are not guessed from the tag value.

Case6 records the actual first X reset at0.107456s and the detached old native
terminal at0.11936s. The pending original therefore exists at reset processing;
its later completion cannot retain bytes or resume the cancelled flow. Source
and relay tag namespaces remain distinct: numeric equality between an input tag
and a swapped output tag is permitted. The separate pool unit exhausts all256
slots and verifies exact guard expiry. Case36 additionally fills the actual
egress pool using255 stateless cleanup transmissions. It checks deletion and
post-guard correlation using real returned FULLs and the existing packet
processor completion counter; it adds no private mutation or test API.

Local recovery expands the saved outer header and enters normal IPv6 processing.
For unsupported transit processing, the model sends NULL and retains fixed
protection; RFC4944 reassembly cannot interpret these compressed offsets.
Stateless reset decoding uses a40-byte routing-only snapshot because reset has
no D. Those bytes are never delivered as a fabricated complete datagram and no
new operation/reverse index is created. A returned reply without a reverse
correlation is discarded.

Execution: all37 debug cases pass both with the default feature set and after
actually disabling the UDP feature and building the core profile. The latter
campaign also passes10 mapped units and2 core modules; the main application and
lifecycle fixtures are excluded because they use UDP applications. Release
comparison also passes: all37 unconditional case rows, diagnostics and the
post-guard tag-reuse measurement match debug. The raw output
claim is adaptation-level bytes, not UDP/application or nested-tunnel service.
Case30's first IPv6 fragment is delivered to normal IPv6 reassembly; it is not
claimed as a complete upper datagram. The byte comparisons intentionally leave
outer header fields reconstructed by an ordinary NHC path outside that path's
raw-tail metadata range.

The deterministic burst/control cases use recorded nonzero backoff exponents to
avoid an exact simultaneous-start diagnostic from the medium. Earlier setup,
checker and stimulus attempts are retained separately. No PHY rule or production
MAC behavior was changed. Release normal completion and printed-field matching
are not substitutes for the debug assertions. No arbitrary-loss liveness or
history beyond the bounded records is claimed.

The same generated case36 inputs fail on the preserved pre-fix library after
the tag guard expires: two FULLs arrive, but the new owner processes no datagram
and sends a sixth probe. With the two allocation-site reverse-index removals,
the reused tag completes with five frames, one FULL and one processed datagram.
The deleted owner is never called during destruction. Exact comparison inputs,
libraries and logs are retained in the verification artifacts.

The final checker moves the distinct-tag insertion outside ASSERT so release
also measures it. Only this fixture was rerun in both modes after that change;
the unchanged sibling results retain their actual inputs. The actual NoUDP
run used the earlier checker, whose insertion already executed in debug. Its
37 rows match the final debug run; this is an explicitly recorded debug
equivalence, not a claim that the final checker bytes were rerun in NoUDP.
