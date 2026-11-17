# Managed non-beacon association

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [managed-applicability.md](managed-applicability.md), [polling.md](polling.md), [indirect-transactions.md](indirect-transactions.md)

## Source and owner contract

IEEE Std 802.15.4-2024, 10.21.2, 10.21.5.1/2, 10.21.6.1.2–5,
Tables 10-101/102 and 10-104–107, Figures 10-151/152/156–158,
and the applicable frame/security/retry clauses govern this selected procedure.
The retrieved source and Figure 10-152 reconciliation are recorded in
[managed-applicability.md](managed-applicability.md). Reset, scan and PAN
selection orchestration belongs to the higher layer. The MAC owns the association
exchange and authoritative PIB updates; the higher layer owns admission,
unique-address allocation, membership policy and duplicate-request policy.

An accepted request selects the PHY channel, sets macPanId and the supplied
coordinator reference, then sends an extended-source Association Request with
broadcast source PAN, AR=1 and device-wide DSN. It may associate only while
unassociated. Failed setup/transmission is terminal; an ACK alone cannot report
association success. After ACK, wait macResponseWaitTime base-superframe
durations, then extract the response with the existing Data Request/ACK/receive
engine. The expected response interval is the documented poll model bound.

The coordinator ignores requests when AssociationPermit=false. Otherwise a
valid request is acknowledged before association indication. Its higher layer
supplies status/address through the response service. A successful response uses
0xfffe when no short address is allocated; rejection carries 0xffff. The
Association Response is extended/extended, compressed PAN, Frame Pending=0,
AR=1 and is queued in the same bounded expiring FIFO as indirect DATA.
It is never sent directly because the request was acknowledged.

Response completion at the coordinator is a correlated native carrier of
COMM-STATUS after actual requested ACK or expiry/abort. It must not appear as
an MCPS-DATA confirmation. PURGE applies only to queued MSDUs, so it cannot purge
association command responses by coincidentally equal handle. Failed indirect
attempts retain response DSN and wait for a new poll. The store advertises the
response's native address through the existing pending-address path.

At the device, validate the response's full destination, selected coordinator
context and active extraction window. Only a valid successful response changes
short address/AssociatedPanCoord and learns the coordinator's extended identity.
Rejection or communication failure resets PAN to its default as Table 10-101
requires. No response, an empty response or an unrelated response is association
failure, never successful admission. A response command terminates the underlying
poll with NO_DATA while association processing has its own result.

The new request shares admission IDs and existing cancellation/reset/stop
barriers. The association timer owns the decision wait and is canceled before
terminal callbacks. Internal polling never allocates an external request ID.
Exactly one association terminal result follows each accepted request; response
commands have their own exactly-one COMM-STATUS carrier. State is detached and
owned packets are destroyed before callbacks. Timers/PHY purposes distinguish
stale work across reset and reentry without a second lifetime-generation counter.

Proposed production evidence covers reset/active-scan/select/join, address and
extended-only success, admission rejection, AssociationPermit=false, lost
request/response ACK, response expiry, shared-store overflow/FIFO, wrong-peer
responses and interruption during request/wait/poll/response. These tests are
not yet implemented. This design does not close the association gate.

## First implementation increment: shared indirect response

Before the device association controller is added, implement the coordinator
response carrier and command extraction path. The native response entry point
takes the higher layer's full device identity, address/status and security
choice. It validates selected status/address combinations, queues an immutable
extended/extended response in the existing store and returns its eventual
COMM-STATUS through a dedicated correlated callback. It allocates no address.
DATA packets retain their current callback/ownership path. Reset/stop detach
both kinds before callbacks; PURGE skips command entries. The command's Frame
Pending remains zero even when more DATA is queued. A polled response command
is acknowledged but completes POLL with NO_DATA and never becomes an MSDU.
This does not yet update device association state or imply successful join.
The production test will exercise a scripted higher-layer response followed by
a real device poll, plus ACK loss, expiration and mixed DATA/command queue cases.
The bounded contract has been checked against all pending completion, purge,
reset/stop, indirect retry and receive/ACK paths before this increment's edits.

## Allocated-address lookup contract

After the higher layer supplies a valid successful response and the shared store
accepts it, the coordinator records the PAN/short-to-EUI relationship implied by
that allocation. This is address lookup state, not an admission algorithm.
Indirect queue keys and poll lookups use the canonical full EUI when the mapping
is known; actual frame addresses retain the service caller's requested form.
The original native address therefore survives wire transmission, while a later
short-addressed poll can extract a frame queued by EUI. Frame Pending count and
failed-attempt retirement use the same canonical key. Unknown short addresses
retain exact native matching. Default RESET clears mappings; retain RESET keeps
them. A subsequent response for the same EUI replaces its previous allocation;
denial/extended-only allocation removes that EUI's short alias. Duplicate short
allocation to another EUI is an INVALID_ADDRESS response-service result, before
queue/PIB mutation. Higher-layer membership/disassociation still owns lifetime
policy and must remove old member transactions when replacing admission.
The proposed production extension joins with short 0x42, queues DATA by full
EUI, polls using 0x42 and verifies the original EUI destination on the wire.
The existing source paths for store insertion, ACK FP, extraction and retained
retry matching were checked before adding this mapping.

For disassociation's required member verification, retain the same higher-layer
allocation decisions as a single PAN/EUI-to-short reference table, including
0xfffe for extended-only members. Canonical short lookup derives from that table;
there is no parallel authoritative membership set or allocation map. It records
the higher layer's accepted successful response, rather than deciding admission.
Default/retain RESET and denied/replaced responses keep the same lifetime policy.
This representation change is required to recognize extended-only members during
disassociation; the existing successful-join/poll cases will be rerun.
