# IEEE 802.15.4 — direct data and receive exchanges

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [applicability.md](applicability.md), [channel-access.md](channel-access.md), [phy-provider.md](phy-provider.md)

Steps 5/6 extend the production `Ieee802154StandardMac` service owner with PHY-client and
access-client roles. It retains one authoritative MAC PIB, one accepted direct request, its
device-wide DSN and retry count. The PHY alone owns radio operation timing; access alone owns
random backoff/NB/BE. Native gate callers use the existing adapter; typed callers use MCPS.
One active direct request is the admission capacity. Additional requests are refused BUSY,
retaining their packets. External packet queues may preserve native tags; no private queue is
invented. The accepted MSDU is destroyed before its exactly one terminal confirmation.

Pre-write control contract: each PHY call has an increasing internal ID and a recorded purpose
before submission, including inline completion. Access asks this MAC for each real CCA. Idle
access leads to TX readiness and a complete serialized version-1 MPDU. A new valid MSDU consumes
one PIB DSN and advances the device counter modulo 256. Retries preserve the frame and DSN,
restart CSMA-CA, and stop after macMaxFrameRetries retransmissions. Success is actual PPDU end
for unacknowledged/group traffic or a matching received immediate ACK. Group/broadcast AR is
cleared on generation. Invalid requests, unsupported security, frame length, access failure
and NO_ACK have separate terminal results.

The matching ACK must have correct FCS, matching DSN, selected channel/context and actual
reception inside the recorded expected interval. Its start cannot precede the transmitted
PPDU end plus turnaround; its end must not exceed the explicit bounded deadline in
[phy-services.md](phy-services.md). Timeout priority +1 follows completion priority 0 at the
same instant. No duplicate suppression is enabled; each valid received retransmission may
be indicated and must still be acknowledged. No exactly-once application claim follows from DSN.

Receive processing uses the audited decision table. It converts both typed and raw frames
through the same byte/FCS boundary, drops simulation-declared bit errors, and applies structural
and FCS checks before ordinary PAN/address filtering. Reserved FCF bit 7 is ignored. Legacy
secured common framing is retained for the required security-policy early returns; protected
bytes are never exposed as plaintext. Foreign valid frames may be monitored in promiscuous
mode without acquiring ACK eligibility. In that mode the raw MHR/payload callback replaces
ordinary MSDU delivery; local ACK and security-error obligations remain independent.

An eligible received AR frame creates a separate short-lived ACK exchange. It owns the held
indication until the ACK is actually sent, so AckSent never means merely scheduled. The target
is received PPDU end plus SIFS. ACK work has priority over a pending local CCA request; a backoff
may expire while ACK work is active, but its requested CCA is deferred until radio readiness and
IFS eligibility. Frames arriving during this MAC's CCA are discarded under 6.3.2.1's permitted
CCA receive policy. ACK work does not complete or cancel an unrelated direct request. A direct
ACK timeout occurring during ACK work records a retry/failure, and subsequent access waits for
the ACK work to finish. No second radio operation is issued concurrently.

The MAC owns a shared IFS eligibility instant, based on complete MPDU size (18-octet SIFS
boundary) and the post-ACK constraints recorded in applicability. Readiness overlaps that wait;
it is not added a second time. Reset/stop detach direct/ACK work, cancel owned timers and retire
internal operation IDs before callbacks. Reset is a barrier and stop takes precedence. The
provider's actual on-air abort limitation remains a release gate; a local aborted confirmation
alone cannot prove that a previously emitted waveform disappeared.

The source/consumer surface is the existing new MAC, its NED/interface composition, native
adapter, paired access/PHY roles, and the standalone PIB's PHY-context refresh. New tests must
exercise production gate/service paths. Planned `tests/protocol/ieee802154/data-ack.test`,
`receive-security.test`, `sequence-wrap.test` and `lifecycle.test`, with corresponding `.md`
reviewer documents, belong to `IEEE802154-6A` (receive/security), `IEEE802154-6B` (immediate ACK)
and `IEEE802154-6C` (direct retries/DSN lifecycle). They must observe real radio exchanges,
filter/rejection results, ACK timing, loss/retry boundaries, 254/255/0/1, and one terminal result.
No such exchange passed before this work. Unit/helper/module results remain supplementary.
