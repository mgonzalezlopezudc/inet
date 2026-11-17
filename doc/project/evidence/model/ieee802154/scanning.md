# Managed non-beacon scanning

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [managed-applicability.md](managed-applicability.md), [phy-provider.md](phy-provider.md)

Current production paths implement passive, ED and active scans. Their executed
evidence and remaining limits are in the [results ledger](results.md). The
contracts below describe the separate increments. Orphan recovery is specified
in [orphan-recovery.md](orphan-recovery.md).

## Passive scan implementation contract

IEEE Std 802.15.4-2024, 6.4.1.1/2, Tables 8-3 and 8-18/19,
requires ascending channel order and beacon-only reception while scanning.
ScanDuration is a uint32 count of 16-us symbols. The native management role
will initially implement PASSIVE; ED, ACTIVE and ORPHAN remain explicit
unsupported admission until their own production paths are implemented.

The MAC remains the sole PHY client and PIB owner. It reads the operating
channel, sets each requested channel, waits for actual receive readiness, and
then starts the channel dwell interval. Zero duration finishes without a
measurement or fictitious beacon. It restores the saved channel before terminal
confirmation, then restores idle receiver policy. Admission excludes active
transactions, queued indirect work and another management request.

Descriptors retain channel, full source identity, PAN, superframe, GTS permit,
pending addresses, opaque payload, LQI and 24-bit timestamp. For the selected
scalar PHY the timestamp boundary is PPDU start plus 160 us, divided into
16-us symbols and reduced modulo 2^24 (6.5.3, macSyncSymbolOffset=0).
Deduplication uses channel/PAN/full source identity. AutoRequest=true stores
descriptors and stops at the implementation capacity with LIMIT_REACHED;
AutoRequest=false emits notifications without retaining a list or early stopping.
Payload-bearing beacons also notify. No pending-address polling occurs in scan.
Only valid unsecured legacy beacons are admitted by this first increment;
secured-beacon failure descriptors remain an explicit obligation.

Cancel and reset cancel the dwell/readiness operation and restore the saved
channel before their local terminal callback. Reset then continues its existing
barrier. Stop cancels work and attempts restoration through the supported
production PHY binding before its shutdown; PHY-only unavailable shutdown
restoration requires separate evidence. No generic alternative asynchronous
PHY lifecycle claim follows. Ordinary stale confirmations are rejected by ID
and purpose. Proposed production tests will send real beacons on multiple
channels and verify sorting, timing, full identity deduplication, filtering,
notification policy, capacity, and channel restoration on completion/cancel/reset.

Contract validated against the MAC's sole PHY-operation dispatcher and the
production provider's inline GET/SET and timed receive-readiness paths before
implementation. The scan timer owns dwell completion; it is not an event used
to defer an ordinary call.

## ED scan extension contract

Clause 10.18 requires repeated ED and a per-channel maximum, while received
frames are discarded. The same MAC owner will save the PHY CCA duration and
temporarily select a one-symbol (16-us) CCA duration while the receiver is off,
then repeatedly perform the PHY's min(128 us, CCA) ED windows. All requested
dwells are integer symbol counts, so no partial ED window or reception-time
CCA write is needed. Restore the original CCA duration and
channel before the terminal callback, including cancel/reset. Temporary internal
CCA writes do not mutate the authoritative MAC backoff PIB; ordinary management
admission remains closed during the scan. A zero-duration ED request performs
zero measurements and reports SUCCESS with an empty energy list and all requested
channels unscanned. It leaves the PHY untouched. This explicitly defines the
zero-measurement boundary permitted by Table 8-18 without fabricating a power
sample: Table 8-19 requires a measurement only for a channel actually searched.
Interrupted ED scans likewise retain unfinished channels in UnscannedChannels.
The proposed production fixture checks peak saturation under actual PPDUs,
quiet channels, 16-us dwells and non-128-us-multiple dwells, and cancel/reset restoration.
This contract was checked against the provider's timed ED and configurable
measurement-window paths before the extension edit.

Completed receive events have priority zero; the scan deadline has priority one.
A beacon completed exactly at the deadline is included. One completed later is
excluded. The packet must also have started after actual receive readiness.
For active scans with access failure on every channel, SUCCESS reports completed
iteration with all channels unscanned; 8.2.8.3 assigns NO_BEACON specifically when
at least one Beacon Request was transmitted. This is not evidence of discovery.

## Protected-beacon failure descriptor extension

Figure 6-3's continuation requires recording information even when unsecuring
fails. The codec already preserves the legacy common header and opaque protected
remainder with a separately verified FCS. The scan owner will retain that header,
BSN, quality/timestamp, complete protected bytes and UNSUPPORTED_SECURITY
(UNSUPPORTED_LEGACY for version 0). It will mark beacon content unavailable and
never interpret protected bytes as superframe, addresses or plaintext payload.
Security parameters are not synthesized from an unimplemented transformation.
Unsecured descriptors retain decoded content. Notification is driven by
AutoRequest=false, NotifyAllBeacons=true, or known nonempty plaintext beacon
payload. This is a bounded failure descriptor, not cryptographic support.
The proposed production cases inject protected frames with valid independently
recomputed FCS and opaque bodies, test notification/list policy, and verify no
decoded plaintext escapes. Empty-beacon cases separately test NotifyAllBeacons.
