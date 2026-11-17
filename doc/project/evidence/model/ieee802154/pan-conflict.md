# PAN ID conflict reporting

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [association.md](association.md), [orphan-recovery.md](orphan-recovery.md)

## Implementation contract

IEEE Std 802.15.4-2024, 10.17.2/3/4.1 and 10.2.10.3/Table 10-4
define detection, command 5 and SYNC-LOSS. Selected operation is non-beacon,
unsecured, with PAN coordinator and directly associated device roles.
The MAC reports conflict; PAN selection/reselection remains higher-layer policy.

A valid decoded beacon with PAN Coordinator set and matching PAN causes the PAN
coordinator to report PAN_ID_CONFLICT directly. A directly associated device
detects it only when the full source differs from both known coordinator
addresses. It queues one outstanding notification context (PAN, coordinator EUI,
actual receive channel) and sends extended/extended compressed command 5 with
AR=1, FP=0 and a device-wide DSN. Notification waits behind an accepted DATA
exchange. Repeated beacons coalesce while that notification is pending. Only
the next DATA admission is refused BUSY while it waits, so reentrant upper-layer
DATA cannot starve it; ordinary GET/SET remain available. Only
its actual matched ACK permits device SYNC-LOSS; failure/reset/stop produces no
false conflict report. The context is detached before the callback and checked
against the current PAN/coordinator relationship. There is no external request
ID for this automatic procedure.

At a PAN coordinator, an unsecured command 5 from a same-PAN full-EUI member
causes ACK and PAN_ID_CONFLICT reporting. Unknown/foreign identities do not.
Received FP is ignored. Extended-only members use the same authoritative member
table. No membership, PAN or PHY configuration changes follow from detection.

Outside scan, the same independent beacon-descriptor helper supplies required
BEACON-NOTIFY for AutoRequest=false, NotifyAllBeacons=true or known nonempty
plaintext payload. Protected bodies remain unavailable-security descriptors
and cannot drive detection from invented plaintext fields. Scan keeps its
existing descriptor-only behavior and does not launch conflicting automatic
traffic during its exclusive channel procedure.

The current early CCA receive guard discards all frames. Beacon processing needs
no ACK or PHY-mode change and is safe while CCA continues; move that rejection
after decode so valid beacons can be described/detected while DATA is backing
off or measuring. Other frame types retain the existing CCA guard in this
increment. DATA/ACK reception overlapping CCA remains a separately required
M1 release check; this beacon change does not claim to resolve it.

Proposed three-node production evidence performs actual reset/scan/join before
injecting a conflicting coordinator beacon. It isolates coordinator/device
detection, ACK success/loss, foreign PAN/non-coordinator/known-parent beacons,
unassociated devices, wrong full member identity, protected input, reset reentry
and queued notification behind a deliberately busy DATA request. Controlled
medium filtering prevents the coordinator from independently hearing the beacon
when testing the member-notification path. These owners, callbacks, IDs and
radio-operation constraints were checked before implementation.
