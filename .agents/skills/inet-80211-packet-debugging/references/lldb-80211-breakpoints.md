# IEEE 802.11 LLDB breakpoint targets

Use `inet-lldb-debugging` for launch, inspection, stepping, watchpoints, and safety. Add breakpoints only after evidence identifies the suspicious transition.

Target the checked-out implementation's equivalents of:

* Queue insertion, QoS classification, and head-of-line selection.
* Channel-access request/grant; backoff start, freeze, resume, and expiration.
* Physical carrier-sense or NAV state changes.
* RTS/CTS/ACK/Block Ack selection, construction, reception, and timeout.
* Retry counter, contention window, retry-limit drop, and duplicate removal.
* Fragmentation, aggregation, deaggregation, and reordering-window updates.
* PHY transmission construction, reception attempt, SNIR/error decision, and radio-state change.
* Scan, authentication, association, roaming, and AP forwarding state changes.

Search symbols first:

```sh
rg -n 'startBackoff|channelAccess|updateNav|retry|drop|computeReception|isReceptionSuccessful|associate|authenticate|BlockAck|aggregate|fragment' \
  src/inet
```

Prefer conditions based on stable packet identity, MAC address, sequence number, retry count, event number, simulation time, or module path. At the stop, record the exchange state before and after the decision, not just the selected branch.
