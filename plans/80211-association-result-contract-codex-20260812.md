# AP association result contract

This note defines the Phase 1 local control path before implementation.

## Ownership

`Ieee80211MgmtAp` owns pending association and reassociation intent. A pending
record contains the peer address, response kind, reserved AID, prior association
snapshot, and the proposed capability/link snapshot. `Ieee80211Mib` owns the
reservation table and the published association/capability state. DCF or HCF
owns the active MAC exchange, including ACK, timeout, retry, and final failure.

## Correlation

The management module allocates a monotonically increasing local transaction ID
and attaches it as `Ieee80211MgmtExchangeTag` metadata to the outgoing response.
The tag is not an `Ieee80211MgmtFrame` field and is never serialized. Retries
preserve the same packet and tag. The MAC result callback returns the ID and a
typed outcome, so stale or duplicate completions are deterministic no-ops.

## Outcomes

The callback reports exactly `ACKNOWLEDGED` or `RETRY_LIMIT_REACHED`. The
coordination function calls it only after the exchange owner has classified the
outcome and before the packet is dropped from the retransmission state. It does
not emit a general observation signal for control flow.

## State transitions

On request, the AP reserves an AID and stages the proposed peer capabilities;
published association state, peer capabilities, and association signals are
unchanged. On `ACKNOWLEDGED`, the MIB atomically publishes the staged capability
snapshot and commits the reserved association, after which the AP emits its
existing association notification. On `RETRY_LIMIT_REACHED`, cancellation,
shutdown, or crash, the AP releases the reservation and discards the staged
record. A reassociation failure therefore retains the previous association.

## Lifecycle and replacement

There is at most one pending transaction per peer. A new request cancels and
aborts the old pending record before reserving a new AID. `stop()` and `crash()`
abort all pending records. A result for an unknown ID or peer is ignored.

## Standards and architecture anchors

- IEEE Std 802.11-2024, 11.3.5.3(o): successful association commits after the
  response is acknowledged.
- IEEE Std 802.11-2024, 11.3.5.5(m): successful reassociation commits after
  the response is acknowledged.
- IEEE Std 802.11-2024, 11.3.4.3(h): successful authentication is not ACK-gated.
- `AR-WLAN-ARCH-OWNERSHIP`, `AR-WLAN-MAC-EXCHANGE`, `AR-WLAN-OBS-EVENTS`, and
  `AR-WLAN-FRAME-REPRESENTATION` govern the ownership, callback, observation,
  and local-correlation boundaries.
