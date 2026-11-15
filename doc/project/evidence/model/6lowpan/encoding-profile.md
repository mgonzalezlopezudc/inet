# 6LoWPAN first-release encoding profile

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [RFC 6282 catalog](../../standard/rfc6282/catalog.md)

This records the P0 stateless selection, optional P8 contexts and P9 extension NHC. Execution evidence is kept separately from this encoding contract. Inline fields follow RFC 6282 order. Decode accepts legal supported encodings even when encode would choose a smaller representation. Missing bytes fail before any read beyond input.

| Field | Encoder selection | Decoder acceptance |
| --- | --- | --- |
| TF=00 | Nonzero flow label and nonzero DSCP; four inline octets | All legal ECN/DSCP/flow values |
| TF=01 | Nonzero flow label and zero DSCP; three inline octets | Reconstruct DSCP zero |
| TF=10 | Zero flow label and nonzero traffic class; one inline octet | Reconstruct flow label zero |
| TF=11 | Traffic class and flow label both zero | Reconstruct both zero |
| HLIM=00 | Hop limit other than 1, 64, 255 | Any inline octet, including those three values |
| HLIM=01/10/11 | Hop limit 1/64/255 respectively | Same fixed values |
| NH=0 | P4: every next header; P5: every non-UDP or fallback header | Inline next-header octet, including extension headers |
| NH=1 | Supported contiguous NHC chain that fits FRAG1 | UDP, extension EIDs 0–4 and nested IPv6 EID 7; reserved EIDs reject |
| CID=0 | Default when both selected context IDs are zero or unused | Use context 0 only for context-dependent fields; ignore unused selectors |
| CID=1 | P8: nonzero selected SCI and/or DCI | Consume the extension byte; source and destination lookups are independent; missing/expired required contexts reject |

For source SAC=0 and unicast destination M=0,DAC=0, use the following table.
Select the shortest valid mode, trying 11, 10, 01, 00 in that order.

| SAM/DAM | Condition for encode | Inline bytes | Decode |
| --- | --- | ---: | --- |
| 00 | Any address not matching a shorter form | 16 | Literal address |
| 01 | Exact fe80::/64 prefix | 8 | Prefix plus inline IID |
| 10 | Exact fe80::/64 prefix and IID 0000:00ff:fe00:XXXX | 2 | Prefix, fixed IID portion, inline suffix |
| 11 | Exact fe80::/64 prefix and IID matches native encapsulating identity | 0 | Derive IID from indicated link source/destination; fail if identity is unavailable |

Extended IID derivation flips the EUI-64 U/L bit; short IID derivation uses
0000:00ff:fe00:XXXX. Compatibility alias bits never participate.
SAC=1,SAM=00 encodes/decodes :: without a context. P8 enables SAC=1 modes 01/10/11 and unicast DAC=1 modes 01/10/11 when the required context is valid. Context bits take precedence over inline or link-derived IID bits; remaining uncovered bits are zero. The encoder selects a context only when compression is permitted and its representation is shorter than the stateless form. Unicast DAC=1,DAM=00 remains reserved.

| Multicast M=1,DAC=0,DAM | Encode condition, shortest valid first | Inline bytes |
| --- | --- | ---: |
| 11 | ff02::00XX | 1 |
| 10 | ffXX::00XX:XXXX | 4 |
| 01 | ffXX::00XX:XXXX:XXXX | 6 |
| 00 | Any remaining multicast destination | 16 |

Decoder accepts each of these forms. P8 enables M=1,DAC=1,DAM=00 for prefix-based multicast using a valid context with at most 64 prefix bits; six inline octets carry flags/scope, RIID/reserved and group ID. DAM=01/10/11 remain reserved. Context permission controls emission only, not decoding.

UDP NHC emits C=0 only. Choose P=11 when both ports are in 0xf0b0..0xf0bf,
otherwise P=01 when destination is in 0xf000..0xf0ff, otherwise P=10 when source
is in that range, otherwise P=00. Decoder accepts all four modes with C=0 and
infers length from uncompressed coverage. C=1 is rejected without integrity support.

Extension EIDs 0–4 represent Hop-by-Hop, Routing, Fragment, Destination and Mobility. NH=0 carries the original next-header octet; NH=1 continues the NHC chain. General length counts compressed body octets and cannot exceed 255. Fragment has seven body bytes and no length field. One trailing Pad1/PadN of at most seven bytes can be removed from Hop-by-Hop/Destination; decoding restores alignment to eight bytes. Nested EID 7 requires NH=0 followed by IPHC. Its elided IIDs copy the enclosing IPv6 addresses' last 64 bits without another U/L inversion. Codec and structural dissector recursion is bounded at sixteen levels.

If a compressed header cannot fit FRAG1, reduce the compressed chain or select
LOWPAN_IPV6, recompute original-byte coverage and alignment, and reject only when no
supported representation fits. Uncompressed extension headers require no NHC support.
