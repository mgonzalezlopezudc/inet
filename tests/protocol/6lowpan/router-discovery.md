# Native Router Solicitation responses

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-completion.md](../../../plan/done/6lowpan-publication-plan.md), [RFC 6775](../../../doc/project/evidence/standard/rfc6775/rfc6775.txt)

Publication association: [C17](../../../plan/done/6lowpan-publication-plan.md#c17); donor `29214e905b`. The retained setup, assertions and parent explanation below describe this behavior. Original run counts, PASS statements and temporary artifact locations refer to the donor cohort. New-prefix results are recorded separately in the publication verification manifest; none is inferred from those historical statements. Exact-parent runtime execution is not claimed unless that manifest records it.


`LowpanRouterDiscovery.test` sends literal Router Solicitations through the native MAC and IPv6 receive path. A router returns a real unicast Router Advertisement with its full native source link-layer address and off-link prefixes. The fixture deliberately configures a conflicting static binding for the soliciting IPv6 address: the RA reaches the actual requesting native peer while the incumbent binding remains unchanged.

The topology contains one forwarding `StandardHost` with the native ND router role and two native NIC peers on PAN 4660. The router uses EUI-64 `0011223344556603`; requester A uses `0011223344556601`, while the static binding points A's link-local IPv6 address to B's `0011223344556602`. No registration is performed. The configured router lifetime is 120 seconds. M/O flags, current Hop Limit 42, Reachable Time 1000 ms and Retrans Timer 200 ms are checked in the resulting RA.

At 4.8 seconds, A sends a valid RS while the interface has advertising disabled; no RA or malformed drop may result. Advertising is re-enabled at 4.9 seconds. Valid multicast RSs at 5 and 8 seconds contain A's native SLLAO. Each response receives an owner-RNG delay in the range 0–2 seconds. An additional RS one simulation tick after the first coalesces with that pending response without postponing it. The router responds to A's IPv6 source and actual native address, using its own link-local IPv6 source, Hop Limit 255 and computed ICMPv6 checksum. Three configured `/64` autonomous prefixes have these wire lifetimes:

| Configuration | RA valid / preferred lifetime |
| --- | --- |
| Absolute expiry at 200 / 100 seconds | Floor of the remaining seconds at RA transmission |
| Relative lifetime 300 / 150 seconds | 300 / 150 seconds |
| Infinite lifetime | `UINT32_MAX` / `UINT32_MAX` |

All three exact prefix addresses (`2001:db8:1::`, `:2::` and `:3::`) are checked. Their configured L bits start set; received PIOs must have L=0 and preserve A=1. Compression is disabled, so each complete advertisement crosses the real radio in two RFC 4944 fragments and is reassembled at A. The producer's `datagramAccepted` event supplies the lifetime snapshot time independently of radio reception delay. At 10.3, 10.4 and 10.5 seconds, bad Hop Limit, checksum and SLLAO/native-source mismatches each produce a malformed drop and no RA.

RSs at 11 and 14 seconds create pending responses that are canceled by crash and graceful shutdown one tick later. After restart, a request at 15 seconds produces a new response. Another RS at 18 seconds is queued, then advertising is disabled; the timer must recheck the flag and remain silent. The static binding is checked before, during and after all exchanges.

The single seed-0 run ends at 20.2 seconds and requires eleven injected frames, three fully checked radio RAs, six transmitted fragments and three malformed drops. The first two observed response delays must differ and each lie strictly above zero and at most two seconds. The peer output must contain no native request tag. Existing `LowpanNeighborRegistration.test` remains a regression control for the ARO server.

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'Lowpan(RouterDiscovery|NeighborRegistration)\.test$'
```

This case proves the router's RS receive and RA transmit paths. It does not prove autonomous host discovery, SLAAC, registration-client retry/refresh, context distribution or multihop DAD.
