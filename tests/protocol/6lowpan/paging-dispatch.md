# Paging and extended dispatch

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-completion.md](../../../plan/done/6lowpan-completion.md), [RFC 8025](https://www.rfc-editor.org/rfc/rfc8025.html), [RFC 8066](https://www.rfc-editor.org/rfc/rfc8066.html)

The adaptation owner recognizes RFC 8025 paging and RFC 8066 ESC selectors
without storing page state between packets. The shared
[LowpanDispatchPrefix](../../../src/inet/linklayer/lowpan/LowpanDispatchPrefix.h)
parser serves Layer entry, Mesh interception, broadcast fragment discrimination,
FRAG1 interpretation and local packet introspection. This adds no module, tag,
configuration parameter, queue or codec API.

Every packet starts on page zero. Consecutive `1111pppp` switches are universal,
even when the current page is unsupported. Parsing stops at the first
nonpaging byte; it does not search unknown payload for a later switch. IPHC has
the same meaning on pages zero and one. Uncompressed IPv6 and ESC retain their
page-zero meaning. Unsupported-page data cannot inherit page-zero semantics.
Interior NALP bytes are not dispatched as a different encapsulated protocol.

The parser retains monotonic first-page-one history through outer parsing.
Mesh and RFC 4944 fragmentation must precede that boundary: switching back to
zero does not make later Mesh/FRAG legal. Thus `f0,Mesh` and `f2,f0,Mesh` work,
`f2,Mesh` is unsupported-page input, and `f1,f0,Mesh` is invalid ordering.
The selected [Mesh/BC0 profile](mesh-forwarding.md) still orders BC0 after Mesh.
It does not invent a separate RFC first-page-one prohibition for BC0:
`Mesh,f1,f0,BC0,IPv6` works, while subsequent FRAG remains forbidden by the
retained first-page-one history.

ESC is exactly `0x40,EET`. There is no generic extension-length byte and no
implemented EET payload protocol. Reserved selectors are rejected; any other
unimplemented EET drops when the endpoint interprets it. No success-path chain
of multiple ESC payloads is claimed. Wire objects expose the selector fields
for serialization/introspection; they do not define extension-specific payload
semantics or make unsupported selectors usable by an application.

## Production paths

```mermaid
flowchart LR
    A["A: literal native sender"] -->|"100m radio"| B["B: Mesh transit or final"]
    B -->|"100m radio: opaque inner bytes"| C["C: interpreting endpoint"]
    A -. "200m below sensitivity" .-> C
```

`LowpanPagingDispatch.test` supplies independent bytes through the native
boundary's public prepared-transmission contract. Actual bounded queues,
IEEE 802.15.4 MACs, narrowband scalar radios and adaptation receivers carry
them. Static domain peers establish identities without learned-neighbor masks.
FreeSpacePathLoss, stationary geometry and receiver sensitivity separate A–B
and B–C from A–C. The fixture observes radio transmissions and rejects any C
MAC-upper reception from A. It does not claim automatic route discovery or
multihop optimized Neighbor Discovery.

For valid Mesh transit, inner unknown EET bytes remain untouched. Leading page
switches and paging between Mesh/BC0 are preserved alongside the reconstructed
Mesh header. B's radio comparison checks the entire payload, changing only
Mesh Hops Left and the immediate native envelope. The same unknown EET bytes
addressed to B are interpreted and dropped there; addressed through B to C,
they cross B's actual radio and are dropped at C without IPv6 delivery.

FRAG1 paging bytes consume encoded wire space and zero original IPv6 octets.
They are removed before existing decompression, leaving original datagram size,
offset and coverage coordinates unchanged. Logical Mesh identities continue to
drive endpoint decompression/reassembly while indication tags remain immediate
receive facts. FRAGN has no dispatch scan: a leading payload `f1,f0,40` remains
ordinary fragment data. The existing NHC grammar also remains separate, so UDP
NHC `f0` and payload `40,f1` cannot become outer paging/ESC.

Unsupported FRAG1 prefixes are refused before range admission. The existing
invalid-first-fragment path reports malformed input; it does not create a
partial IPv6 interpretation or skip an unknown extension. The two partial
expirations below arise from deliberately valid FRAGN-only leftovers sent
after rejected first fragments. They are not rejected-FRAG1 state.

## Direct fixture cases

All three simulations use General configuration and seed zero. Endpoints expose
byte-exact adaptation output; the fixture does not hand those bytes to UDP or
an application and does not prove transport checksum/application delivery.

| Run | Independent stimuli | Observed result |
| --- | --- | --- |
| 0 | 30 inputs: implicit/page-one IPHC; reset with a new uncompressed packet; repeated switches and unsupported-page recovery; leading paging before Mesh; valid BC0 after first page one with return zero; unsupported page/type, reserved/truncated ESC/paging/IPHC, no unknown-EET payload scan, invalid first-page-one order, interior NALP and UDP NHC `f0`. | 13 byte-exact B deliveries, 17 expected endpoint drops, no forwarding or fragment processing. A new packet cannot inherit the preceding page. |
| 1 | 14 inputs: paged FRAG1 plus raw FRAGN, Mesh fragments from distinct immediate peers, page-zero prefix before FRAG headers, broadcast paged-fragment discrimination, forbidden first-page-one FRAG and nested Mesh, BC0-retained ordering history and unsupported EET within FRAG1. | Four byte-exact reassemblies, four expected drops and two intentional FRAGN-only expirations. FRAGN paging-valued bytes survive unchanged. |
| 2 | Seven inputs: identical unknown EET bytes through B to C, with no prefix/leading `f0`/leading `f2,f0`; same EET addressed to B; unsupported `f2,Mesh`; invalid `f1,f0,Mesh`; valid paged-BC0 broadcast. | Four actual B radio forwards, three C endpoint drops and three B endpoint drops. Broadcast delivers once at B and C. Exact comparison preserves every paging/EET byte and changes only Mesh hop/address envelope facts. |

The companion `LowpanPagingWire.test` supplies literal Page/ESC vectors, all
page selector values, universal recovery and packet-reset checks, monotonic
ordering across BC0, and ordered dissector/printer observations. The dissector
leaves FRAGN and unknown extension payload opaque; it cannot infer a generic
EET payload length. The unchanged IPHC/NHC and Mesh units are controls.

## Verification

Matching debug execution passed 19 protocol fixtures and 119 simulations, seven
unit fixtures and seven module fixtures. Release completed five protocol fixtures
and 45 simulations. The 43 printed Paging/Mesh/updated-host counter rows matched
debug exactly; the other two release fixtures establish normal completion.
Both builds and architecture, naming, explicit source-protection and whitespace
gates passed. The new fixture required no setup or source repair after its first
successful build/run.

Fresh matching debug/release builds, scoped protocol/unit/module results,
commands, source/generated/binary/library hashes and raw artifacts are in
[p13-paging-verification.json](../../../doc/project/evidence/model/6lowpan/p13-paging-verification.json).
Debug provides semantic assertions. Release completion and independently
compared printed counters do not activate disabled C++ ASSERTs. This report
does not claim an implemented EET payload, successful multiple-ESC continuation,
dynamic mesh routing, transport delivery, multi-seed robustness or a
parent-revision runtime comparison of the new feature.
