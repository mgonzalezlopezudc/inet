# Routed authority lifetime at the clock horizon

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-completion.md](../../../plan/done/6lowpan-publication-plan.md), [RFC 6775 §8.2.4](https://www.rfc-editor.org/rfc/rfc6775.html#section-8.2.4)

Publication association: [C25](../../../plan/done/6lowpan-publication-plan.md#c25); donor `d865f9f6bd`. The retained setup, assertions and parent explanation below describe this behavior. Original run counts, PASS statements and temporary artifact locations refer to the donor cohort. New-prefix results are recorded separately in the publication verification manifest; none is inferred from those historical statements. Exact-parent runtime execution is not claimed unless that manifest records it.


**Execution status: debug PASS, one run.** This fixture tests the ordinary-interface routed authority independently of native admission, a 6LN client or a 6LR relay. It uses no authority-helper private API.

```mermaid
flowchart LR
    R["Literal DAR requester"] <-->|Ethernet| T["IPv6 transit router"] <-->|Ethernet| B["Ordinary-only 6LBR\ncapacity 1"]
```

At femtosecond resolution `SIMTIME_MAX` is approximately 9223 seconds. A maximum DAR lifetime, 65535 minutes, exceeds the representable relative duration. The authority must compare raw seconds with the remaining horizon before constructing a `SimTime`, then store the saturated absolute deadline. The fixture operates between 0.1 and 0.9 seconds, with explicit 120/60-second advertised prefix lifetimes so configurator defaults do not overflow this deliberately fine clock. It has no radio or large absolute simulation timestamp.

Eight independent literal DAR bodies enter Ethernet encapsulation, cross the transit router, then reach real authority IPv6/ND processing. Two requests use lifetime 65535 and receive real routed DACs echoing that value. Different-owner probes receive Duplicate Address without displacing the incumbent; matching zero lifetimes remove it, allowing another owner to register. A final ordinary one-minute registration and zero removal leave the table empty. The expected authority-count sequence is `1,0,1,0,1,0`. All eight request and reply packets cross the transit router; Hop Limit 64 becomes 63. DAC bytes, registered address, owner, status, lifetime and computed checksum are checked against literal expected bytes. The authority has no native interface and creates no ND-owned `/128` route.

Focused reproduction command (also passed in an isolated four-fixture selection before native border admission):

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/6lowpan$' -f 'LowpanAuthorityValidity\.test$'
```

The observed totals are eight DARs, eight DACs and authority counts `1,0,1,0,1,0`. All eight DARs reached both the transit router and authority; all eight DACs crossed the transit router and reached the requester.

Matching debug and release builds passed. Raw logs, decoded runtime observations, the minimal standalone patch and source hashes are retained under `/home/miguel/workspace/.codex-artifacts/lowpan-p10-border-20261116/`. Debug assertions check the protocol result; release runtime assertions are not used as proof.

The unchanged fixture fails against parent `5b34aeb425` at event 38, time 0.10003156 seconds: constructing a 3932100-second duration exceeds the femtosecond clock range. Applying only the two-line lifetime correction passes in the isolated checkout, together with routed authority, host registration and malformed-wire controls. Matching debug/release builds and scoped gates pass. The [standalone record](https://github.com/mgonzalezlopezudc/inet/blob/9908c007bdb1eda9d74f6ca1f99fe1cae33fbc61/doc/project/evidence/model/6lowpan/p10-authority-validity-verification.json) retains exact commands, source hashes and both runtime results.

The accepted maximum-lifetime requests directly prove that duration construction did not overflow. Ownership/removal probes establish table behavior through the public wire path. The exact stored `SIMTIME_MAX` deadline is established by the bounded source expression; the test does not inspect a private table or wait until the clock horizon. No release runtime assertion is claimed here.
