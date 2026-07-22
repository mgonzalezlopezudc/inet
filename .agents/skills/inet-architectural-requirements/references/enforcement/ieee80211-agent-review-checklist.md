# IEEE 802.11 Agent-Review Checklist

Use this checklist together with the general
[`agent-review-checklist.md`](agent-review-checklist.md).

Review only the changed code. Do not re-audit unrelated existing code.

For every item, output one of:

* `PASS — <requirement>`
* `FLAG — <requirement> — <file:line> — <clear violation>`
* `QUESTION — <requirement> — <file:line> — <reason human judgment is needed>`

Use `FLAG` only for a clear violation. Use `QUESTION` for plausible but uncertain architectural problems.

Do not flag deviations already recorded in `architecture-exceptions.md`.

## Checklist

### Standard fidelity

**[AR-WLAN-STD-TRACE] Is normative behavior traceable to a standard revision and clause?**

FLAG a new timing rule, state transition, frame constraint or protocol decision with no identifiable normative source.

**[AR-WLAN-STD-GATING] Is new behavior gated by operation mode and negotiated capabilities?**

FLAG amendment-specific behavior that automatically affects legacy modes, or capability detection based on concrete class type, module path or incidental frame fields.

### Responsibilities and state

**[AR-WLAN-ARCH-BOUNDARIES] Does each component stay within its responsibility?**

FLAG MAC code that calculates PHY details, PHY code that makes scheduling decisions, rate control that mutates channel-access state, or management code that directly manipulates MAC internals.

**[AR-WLAN-ARCH-OWNERSHIP] Has mutable protocol state acquired more than one owner?**

FLAG duplicated association, sequence, retry, NAV, backoff, TXOP, Block Ack, reorder, aggregation, QoS or power-save state.

**[AR-WLAN-ARCH-VARIANTS] Is a substantial variant implemented as scattered conditions?**

FLAG repeated amendment, station-role or algorithm checks distributed through common classes when the behavior should be a replaceable policy or component.

### Frames and PHY

**[AR-WLAN-FRAME-REPRESENTATION] Is on-air information represented as typed packet content?**

FLAG wire information stored only in tags, local metadata serialized into frames, duplicated frame-classification logic, missing introspection support, or manual edits to generated message files.

**[AR-WLAN-PHY-AUTHORITY] Is PHY mode information calculated by the authoritative mode implementation?**

FLAG duplicate rate tables, duration formulas or legality checks in MAC, rate-control or scheduler code.

**[AR-WLAN-PHY-TIMING] Are timing values derived and centralized?**

FLAG hardcoded interframe spaces, slot times, ACK timeouts, PPDU durations or duplicated symbol-rounding calculations.

### MAC behavior

**[AR-WLAN-MAC-EXCHANGE] Is each exchange controlled by one explicit state machine?**

FLAG response, timeout or retry decisions spread among unrelated components; multipurpose timers; approximate backoff restart; or event ordering that depends on accidental scheduling order.

**[AR-WLAN-MAC-SEQUENCE] Are sequence and Block Ack operations wrap-around safe and centralized?**

FLAG raw sequence-number ordering, reassignment of sequence numbers during retransmission, duplicated reorder-window state or aggregation that loses constituent MPDU identity.

**[AR-WLAN-MAC-QOS] Are TID mapping and per-access-category state owned centrally?**

FLAG repeated TID-to-AC classification, shared mutable contention state between access categories, or internal-collision behavior determined accidentally by queue or event order.

**[AR-WLAN-MAC-MULTIUSER] Is scheduling separated from PPDU construction?**

FLAG PHY code selecting users, a scheduler mutating PHY internals, resource allocation represented through scattered parameters, or missing validation of an MU transmission plan.

### Observation and verification

**[AR-WLAN-OBS-EVENTS] Are semantic events emitted once by their owner?**

FLAG duplicate emission of one logical event, statistics calculated inside protocol decisions, or observers that affect simulation behavior.

**[AR-WLAN-QUAL-TESTS] Does the change include focused correctness tests and legacy coverage?**

FLAG new normative behavior tested only through fingerprints, missing boundary tests, missing capability combinations, or no regression test showing that unaffected legacy modes retain their behavior.

Also FLAG deterministic decisions that depend on pointers, allocation order, thread order or unordered-container iteration.

## Required Output

Finish with:

`WLAN REVIEW: <n> PASS, <n> FLAG, <n> QUESTION`

For every `FLAG`, include one concrete correction. Propose an exception-ledger entry only when the deviation appears deliberate and cannot reasonably be corrected.
