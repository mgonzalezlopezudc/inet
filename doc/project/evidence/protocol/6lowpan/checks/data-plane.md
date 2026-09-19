# 6LoWPAN data-plane checks

> **Kind:** procedure · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [features.md](../features.md), [RFC 4944 catalog](../../../standard/rfc4944/catalog.md), [RFC 6282 catalog](../../../standard/rfc6282/catalog.md)

Use two endpoints A and B on one PAN; add router R and a second PAN for forwarding.
Observe supplied IPv6 bytes, transmitted frames and delivered IPv6 bytes independently.
Use asymmetric addresses and payloads, a fixed fault schedule, and a bounded observation
window. Each absence assertion must first establish arrival of its stimulus.

## Link and dispatch

Sources: RFC 4944 §§2–5, RFC 6282 §5, RFC 8025 and RFC 8066.

1. Send a fitting IPv6 packet using dispatch 0x41. Remove only adaptation encapsulation
   and require identical delivered IPv6 bytes.
2. Send IPv6 multicast. Require short broadcast destination 0xffff with the link PAN.
3. Supply empty, truncated, unknown and unsupported dispatch encodings. No malformed
   packet may become IPv6. Classify 0x7f as IPHC, not the obsolete ESC assignment.
4. Deliver a 1280-octet IPv6 packet over the link and verify complete reconstruction.

## Fragmentation and reassembly

Sources: RFC4944-FRAG-1 through FRAG-5, REASM-1 through REASM-3; RFC6282-FRAG-1.

1. Vary size on both sides of each supported frame capacity and eight-octet boundary.
   Check original size and offsets, non-final alignment, and complete byte coverage.
2. Deliver fragments in order, reverse order and with FRAGN first. Interleave senders,
   destinations and tags; no fragment may enter another identity's assembly.
3. Lose the final fragment, duplicate a fragment, and deliver one just before, at and
   after the reassembly deadline. No partial datagram may be delivered. Timeout begins
   on the first received fragment and is no greater than 60 seconds.
4. Introduce overlap with different range boundaries. Verify accumulated fragments are
   discarded. A new assembly with the incoming fragment is permitted by RFC 4944 §5.3.
5. Trigger disassociation while reception and transmission are partial. Require partial
   received and not-yet-transmitted fragments in the affected scope to be discarded.
6. Observe tags 65534, 65535 and 0 on successive fragmented datagrams. Repeat a complete
   identity/size/tag key with conflicting and nonconflicting ranges; the latter cannot
   reveal generation identity from the fragment header alone.

Exact duplicate handling, conflicting same-range bytes, storage admission and tag reuse
beyond these requirements need explicit implementation policies; they are not additional
normative RFC requirements.

## Stateless IPHC

Sources: RFC 6282 §§2, 3.1.1, 3.1.2, 3.2.1 and 3.2.2.

1. Independently encode every TF and HLIM value and each stateless SAM/DAM form, the
   unspecified source and all four stateless multicast forms. Check inline field order,
   lengths, ECN/DSCP placement, and reconstructed bytes.
2. Exercise both native address widths, unequal source/destination values, exact IID
   matches and mismatches. Elision must not invent IID bits.
3. Exercise CID present/absent, context-dependent and reserved combinations. Distinguish
   unsupported contexts from the context-independent unspecified-source encoding.
4. Truncate each inline field at every byte boundary. Malformed data must not produce
   a partially reconstructed packet.
5. Fragment compressed packets: offsets and datagram size refer to original bytes.
   All compressed headers fit FRAG1; headers that cannot fit must not be compressed.
   Check uncompressed fallback with header bytes extending into later fragments.

## UDP and forwarding

Sources: RFC 6282 §4.3; RFC 8200 §§3, 4.5 and 5; RFC 4443 §3.2.

1. Cover all four port modes, each compressed range endpoint and its neighboring ports.
   Verify UDP length and carried checksum with independently constructed expected bytes.
2. With checksum elision authorization/integrity absent, require a carried checksum on
   transmit and rejection of received C=1 packets.
3. Across A–R–B, observe full reassembly, IPv6 Hop Limit decrement, next-hop selection
   and a fresh outgoing adaptation exchange. No direct forwarding of RFC 4944 fragments.
4. A datagram at or below 1280 needs no IPv6 Fragment header solely for adaptation.
   Forwarding a larger datagram onto that IPv6 MTU produces Packet Too Big; source
   fragmentation is a separate operation before adaptation.
