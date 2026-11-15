# Native receive state across restart

> **Kind:** report · **Status:** snapshot 2026-11-15 · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-implementation-plan.md](../../../plan/pending/6lowpan-implementation-plan.md), [data-plane.md](../../../doc/project/evidence/protocol/6lowpan/checks/data-plane.md)

The P7 lifecycle row needs a native protocol sequence with pending RX state and late fragments. `LowpanNativeReceiveRestart.test` adds that control for the existing stop/crash cleanup. Its planned commit is `tests: add: native 6LoWPAN receive restart coverage`. No production defect is repaired: the real adaptation module already clears its reassembly table and cancels its timer on both operations.

Two native interfaces share a static PAN and scalar medium. Literal frame bytes enter the first interface's native MAC, boundary and adaptation path. First ranges for two independent 80-byte datagrams arrive at 1 and 2 ms. `ScenarioManager` shuts down the receiver in run 0 or crashes it in run 1 at 5 ms, then starts it at 10 ms. A separate `NodeStatus` observation proves DOWN at 6 ms and UP at 10.5 ms; a missing lifecycle stimulus cannot pass as successful cleanup.

An old tail at 11 ms must not complete the old first range. A newly injected matching first range permits byte-exact delivery at 12 ms. A fresh tag completes another datagram at 14 ms. The other old tail, arriving at 15 ms, must create an incomplete new context and expire at 115 ms. Its original pre-stop first range must neither cause delivery nor leave an expiry at 101/102 ms. Each run requires seven fragments observed by adaptation, exactly two completions/deliveries, only the 115 ms expiry, and no drops.

This deliberately preserves the RFC limitation: a late fragment and fresh fragments with the same complete wire identity can be indistinguishable. Accepting the matching first range at 12 ms is valid; the test does not invent a generation field or claim immunity to all stale-fragment combinations. Static PAN membership and neighbors remain configured across restart. Association/disassociation remains outside the modeled profile. Injection isolates native receive cleanup; it does not measure radio loss or pending-TX behavior, which has separate lifecycle coverage.

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'LowpanNativeReceiveRestart\.test$'
```

Run from the INET root with a matching debug library. The single fixture executes both lifecycle variants with seed 0 and a 130 ms limit. Exact hashes and results are in the [closure record](../../../doc/project/evidence/model/6lowpan/p7-closure.json). The parent's unchanged production code passes the same assertions; this is added coverage, not a failing-parent comparison.
