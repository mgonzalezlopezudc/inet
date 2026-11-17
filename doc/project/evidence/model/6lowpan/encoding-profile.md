# 6LoWPAN first-release encoding profile

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [RFC 6282 catalog](../../standard/rfc6282/catalog.md)

This is the P0 implementation selection, not evidence of implemented behavior. Inline
fields follow RFC 6282 order. Decode accepts legal supported encodings even when encode
would choose a smaller representation. Missing bytes fail before any read beyond input.

| Field | Encoder selection | Decoder acceptance |
| --- | --- | --- |
| TF=00 | Nonzero flow label and nonzero DSCP; four inline octets | All legal ECN/DSCP/flow values |
| TF=01 | Nonzero flow label and zero DSCP; three inline octets | Reconstruct DSCP zero |
| TF=10 | Zero flow label and nonzero traffic class; one inline octet | Reconstruct flow label zero |
| TF=11 | Traffic class and flow label both zero | Reconstruct both zero |
| HLIM=00 | Hop limit other than 1, 64, 255 | Any inline octet, including those three values |
| HLIM=01/10/11 | Hop limit 1/64/255 respectively | Same fixed values |
| NH=0 | P4: every next header; P5: every non-UDP or fallback header | Inline next-header octet, including extension headers |
| NH=1 | P5: supported UDP NHC that fits FRAG1 | P5: UDP NHC only; other NHC rejected as unsupported |
| CID=0 | Always | Accept with supported address combinations |
| CID=1 | Never in first release | Consume extension byte; accept when no address requires context, ignore unused selectors; reject context-dependent forms |

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
SAC=1,SAM=00 encodes/decodes :: without a context. Other SAC=1 modes are unsupported.
M=0,DAC=1,DAM=00 is reserved; other M=0,DAC=1 modes require unsupported contexts.

| Multicast M=1,DAC=0,DAM | Encode condition, shortest valid first | Inline bytes |
| --- | --- | ---: |
| 11 | ff02::00XX | 1 |
| 10 | ffXX::00XX:XXXX | 4 |
| 01 | ffXX::00XX:XXXX:XXXX | 6 |
| 00 | Any remaining multicast destination | 16 |

Decoder accepts each of these forms. M=1,DAC=1,DAM=00 requires a context and is
unsupported; DAM=01/10/11 are reserved. No stateful mode is emitted.

UDP NHC emits C=0 only. Choose P=11 when both ports are in 0xf0b0..0xf0bf,
otherwise P=01 when destination is in 0xf000..0xf0ff, otherwise P=10 when source
is in that range, otherwise P=00. Decoder accepts all four modes with C=0 and
infers length from uncompressed coverage. C=1 is rejected without integrity support.

If a compressed header cannot fit FRAG1, reduce the compressed chain or select
LOWPAN_IPV6, recompute original-byte coverage and alignment, and reject only when no
supported representation fits. Uncompressed extension headers require no NHC support.
