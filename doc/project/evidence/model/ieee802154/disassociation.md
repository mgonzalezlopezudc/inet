# Non-beacon disassociation

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [association.md](association.md), [indirect-transactions.md](indirect-transactions.md)

## Implementation contract

IEEE Std 802.15.4-2024, 10.21.3, 10.21.5.3, Tables 10-103 and
10-108–110 govern this increment. The MAC owns frame exchange and its native
PIB/member references. Higher-layer admission/allocation supplied the member
records; the higher layer removes its own policy references when notified.

Validate same-PAN destination, unicast native identity, selected reason 1/2 and
unsecured service parameters. A device leaving its configured coordinator uses
direct transmission regardless of TxIndirect. A coordinator targeting a known
member uses the requested direct/indirect method. Membership includes 0xfffe
extended-only records. Unknown members and foreign PAN requests fail before
queue or identity mutation. Generated commands use extended source, selected
destination mode, compressed PAN, AR=1, Frame Pending=0 and device-wide DSN.

Direct exchange reuses the transaction engine; indirect commands share the same
immutable bounded FIFO/expiry/poll mechanism as DATA and Association Response.
Command receipt completes POLL with NO_DATA without an MSDU. PURGE skips commands.
Actual protocol success or transmission/access/expiry failure applies the chosen
10.21.3 policy: the requester considers the device disassociated. Local cancel,
RESET and STOPPED abort the local request without inventing a protocol outcome;
they retain the existing emitted-waveform limitation.

An incoming notification from the configured coordinator clears the device's
five association PIB references. At a coordinator, another source is accepted
only if its full extended identity is in the same-PAN member table. Unknown
sources are ignored. Receiver cleanup follows ACK completion and precedes the
native disassociation indication. Frame Pending is ignored on reception.

Coordinator cleanup removes its peer record and all queued transactions for
that canonical identity; active exchange for the peer is locally aborted with
DISASSOCIATED. The device clears its own outstanding transactions/poll and
five PIB references. Selected queue entries and packets are detached/destroyed
before callbacks. A bounded peer-cleanup barrier refuses new service admission
with BUSY, including reentrant RESET; module stop still drains other state, and
STOPPED wins for remaining callbacks. Other peers' queues survive ordinary
disassociation. The native confirmation echoes the requested peer/PAN and result
for correlation; it is not a wire encoding.

The proposed production test first performs real scan/join, then exercises
device-initiated direct leave, coordinator direct/indirect removal, short and
extended-only identities, lost ACK/access failure/expiry, foreign/unknown sender
rejection and peer queue cleanup. It checks no stale PAN/coordinator reference
and no extra DATA/command terminal callback. Existing join, shared-store and
lifecycle cases must remain passing. These paths and the ownership/callback
boundaries were checked against the current source before implementation.
