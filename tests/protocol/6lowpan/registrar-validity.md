# Native registrar target and lifetime validity

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-completion.md](../../../plan/done/6lowpan-completion.md), [RFC 4862 §2](https://www.rfc-editor.org/rfc/rfc4862.html#section-2), [RFC 6775 §6.5](https://www.rfc-editor.org/rfc/rfc6775.html#section-6.5)

**Execution status: debug PASS, two runs.** `LowpanRegistrarValidity.test` directly tests the existing local registration server. It does not require border-router admission or a routed DAR relay.

```mermaid
flowchart LR
    P["Native peer\nEUI-64 ...6601\nregistered address 2001:db8::a"] <-->|"PAN 4660\nliteral NS/ARO in; real radio NA out"| R["Native registrar\nEUI-64 ...6603\nLowpanIpv6NeighbourDiscovery router role"]
```

Both runs use the production native MAC, adaptation, IPv6 and ND receive path. Literal request frames enter the router MAC's receive gate; the resulting NAs cross the actual IEEE 802.15.4 medium and are decoded by the peer. The domain contains no static `<neighbor>` binding. The router owns each accepted `/128` route and learned native binding. Compression is disabled, seed is zero, and simulation resolution is explicitly one femtosecond. Scalar and vector recording are disabled; direct listeners and assertions provide the observations.

Run 0 assigns a non-tentative local target alias, `2001:db8:fe::1`, whose valid lifetime ends at one second. A one-minute registration request at 0.9 seconds succeeds. A real radio NA must contain status zero, lifetime one, the alias as source/target, the requesting registered IPv6 destination, Router/Solicited flags and a computed checksum. Before expiry, the public domain lookup and the router's ND-owned `/128` route must both be present, with route deadline 60.9 seconds.

Otherwise identical refresh requests arrive at exactly one second and at 1.1 seconds. The fixture verifies that the alias row still exists while its lifetime query reports invalid. Both requests must produce observable malformed drops and no further NA; the initial binding and route deadline remain unchanged. This distinguishes current address validity from mere address membership and tests the exact boundary without relying on cleanup of the address row.

Run 1 uses the registrar's permanent native link-local target and sends at 0.9 seconds. With femtosecond resolution the clock horizon is about 9223 seconds, so the maximum wire lifetime, 65535 minutes, cannot itself be represented as a relative duration. The request must return a real radio success NA echoing that value. Its domain binding remains resolvable and its owned route expires at `SIMTIME_MAX`. The deadline computation compares the raw relative seconds with the available clock horizon before constructing or adding a duration, so the request cannot overflow the clock. A matching zero-lifetime request at one second returns a zero-lifetime NA and removes the route and binding. This preserves zero as immediate removal rather than treating it as infinity.

The fixture explicitly ends run 0 at 1.2 seconds and run 1 at 1.1 seconds. Observed debug totals:

| Run | Inputs | Real radio NAs | Malformed drops | Registration counts |
| --- | ---: | ---: | ---: | --- |
| Finite target | 3 | 1 | 2 | 1 |
| Clock horizon and removal | 2 | 2 | 0 | 1, 0 |

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'Lowpan(RegistrarValidity|NeighborRegistration)\.test$'
```

This selection passed two fixtures and three runs. The existing registration fixture controls owner conflicts, capacity, ordinary NS/DAD replies, normal expiry and restart; its observed totals were 21 inputs, 11 ARO NAs, one delivered data packet and six malformed drops. Source inspection predicts that the parent accepts the expired-target refreshes and overflows while constructing the maximum relative duration; no executed failing-parent comparison is claimed here. This case does not establish native border-router policy, DAR relay timing or host retry behavior.

Raw logs, decoded packet observations and the verification record are retained under `/home/miguel/workspace/.codex-artifacts/lowpan-registrar-validity-20261116/`. The debug runner uses active assertions for packet fields, checksums, exact deadlines and retained state. A release build checks compilation; release fixture assertions are not claimed as semantic proof because `NDEBUG` disables them.

An earlier diagnostic used picosecond resolution and a timestamp near 9.22 million seconds. It reached registration and NA transmission, then failed inside the analog radio's interval calculation. Before disabling statistics, that setup also spent excessive time filling throughput bins across the virtual jump. Both diagnostic logs are retained; the large absolute timestamp is **not validated** by this fixture. The final normal-epoch test directly exercises clock-horizon saturation without that unrelated analog-model limitation.
