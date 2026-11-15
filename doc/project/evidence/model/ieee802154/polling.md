# IEEE 802.15.4 polling implementation contract

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [managed-applicability.md](managed-applicability.md), [indirect-transactions.md](indirect-transactions.md)

Step 7 connects the shared pending store to `Ieee802154StandardMac`. An explicit
`enableManagement` parameter selects the expanded receive capability; the bounded
M1 composition retains its direct-data selection. The optional paired management
role adds typed POLL and PURGE without making existing MCPS clients implement
unrelated management callbacks. Both roles share the MAC's monotonically increasing
request-ID space and cancellation/reset/lifecycle barriers.

Table 8-30 requires ignoring IndirectTx at a non-coordinator or when the destination
is omitted; those requests enter the direct service. The current managed capability
supports unicast indirect delivery. Broadcast/group indirect requests report
UNSUPPORTED_FEATURE until their beacon-release mechanism is integrated, rather than
silently dropping a valid option. This restriction blocks a broader managed-profile claim.

Accepted indirect MCPS packets remain owned by the MAC until extraction, purge,
expiry, reset or stop. Queue overflow completes TRANSACTION_OVERFLOW. The entry's
immutable serialized frame and DSN survive failed extraction. Successful ACK or
unacknowledged transmission removes it and completes once. No direct retry is
scheduled for an indirect failure. PURGE removes only a matching queued handle;
an active extraction is not purgeable (Tables 8-33/34). The native API additionally
retires the accepted DATA request with local PURGED before its PURGE confirmation;
this preserves its exactly-one-terminal contract without inventing an IEEE DATA status.
Pending expiry is
processed before poll Frame Pending calculation, including equality at the deadline.

The coordinator ACKs a valid Data Request and computes FP from the matching full
PAN/address key. After ACK completion, a queued extraction uses ordinary unslotted
CSMA-CA; this selects the second mechanism in 10.22.3 and makes no immediate-access
claim. DATA FP reports further entries for the same device. Every successfully
received poll earns at most one response attempt. An empty store reports ACK FP=0.

The device transmits a real command, waits for its matching ACK, and reports NO_DATA
for FP=0. FP=1 keeps RX enabled until matching response or the configured expected
response deadline. Zero-length DATA gives NO_DATA without an MSDU indication;
nonempty DATA gives the ordinary MCPS indication and poll SUCCESS. A different explicit
source cannot satisfy the poll. An address-omitted source is correlated through the
accepted destination/PAN and response interval; unsecured operation does not authenticate
its coordinator. An accepted response's ACK processing finishes before its
indication, preserving the existing actual AckSent semantics.

`pollResponseWait` is an explicit model bound for the coordinator's CSMA-CA response,
not an IEEE constant. Its configured value must cover the selected peer access and
propagation assumptions. Transaction persistence instead uses the exact Table 8-36
PIB units, 960 selected symbols per period at BO=15. Stop/reset must detach all
pending ownership and timers before callbacks; expiry/purge/cancel must not produce
a second completion from a late ACK. The existing indivisible on-air signal limit
still applies and is not hidden by local cleanup.

A multi-entry reset detaches the entire pending map and destroys every retained
packet before its first notification. Otherwise that callback could cancel another
barrier-owned request and change its RESET outcome. A notification-drain flag defers
reset completion if stop reenters from an abort callback: remaining requests receive
STOPPED before the reset's STOPPED confirmation. The flag owns only this callback
ordering interval; it is not a second transaction state. The production fixture
includes cancellation probes during two-entry reset and stop from its first callback.

Production evidence and its limits are in [indirect.md](../../../../../tests/protocol/ieee802154/indirect.md):
44 debug runs pass across typed/raw radios, empty/single/multiple queues, identity
separation, overflow, expiry, poll/response loss, two sleeping devices and queued
retirement, exact poll/expiry equality and multi-entry reset/stop reentry.
Active-extraction interruption, broadcast release and shared association-response
integration remain open; step 7 is not yet complete.
