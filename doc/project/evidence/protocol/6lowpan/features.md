# 6LoWPAN feature map

> **Kind:** what · **Status:** current · **Seal:** none · **Owns:** `LOWPAN-F-*` · **Stands on:** [standards.md](standards.md)

This map describes standards capabilities, not implementation or achieved test depth.

| ID | Capability | Normative source |
| --- | --- | --- |
| LOWPAN-F-LINK | IPv6 MTU 1280, link broadcast for multicast, native link identities | RFC 4944 §§2–4, 6 |
| LOWPAN-F-DISPATCH | Uncompressed IPv6 dispatch and ordered encapsulation headers | RFC 4944 §5; RFC 6282 §5; RFC 8025; RFC 8066 |
| LOWPAN-F-FRAG | Original-datagram size, tag and eight-octet offsets | RFC 4944 §5.3; RFC 6282 §2 |
| LOWPAN-F-REASSEMBLY | Address-scoped assembly, overlap discard, bounded timeout and disassociation cleanup | RFC 4944 §5.3 |
| LOWPAN-F-IPHC | Stateless traffic class, flow label, hop limit and address encodings | RFC 6282 §§3.1–3.2 |
| LOWPAN-F-UDP | Four port encodings, inferred length and carried checksum | RFC 6282 §4.3 |
| LOWPAN-F-CONTEXT | Context-based unicast and multicast compression | RFC 6282 §3 |
| LOWPAN-F-NHC | Extension-header compression chains | RFC 6282 §§4.1–4.2 |
| LOWPAN-F-MESH | Mesh addressing and forwarding | RFC 4944 §§5.2, 11 |
| LOWPAN-F-ND | Registration and compression-context distribution | RFC 6775; RFC 8505 |
| LOWPAN-F-RECOVERY | Fragment forwarding and selective recovery | RFC 8930; RFC 8931 |

## Checks

The [data-plane procedures](checks/data-plane.md) cover the basic link, dispatch,
fragmentation, reassembly, IPHC and UDP capabilities. The [standards map](standards.md)
records the wider family; this feature map does not imply exhaustive checks for later
capabilities. Catalogs remain the source of individual normative obligations.
