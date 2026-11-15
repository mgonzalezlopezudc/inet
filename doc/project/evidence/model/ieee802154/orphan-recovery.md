# Non-beacon orphan recovery and realignment

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [scanning.md](scanning.md), [association.md](association.md)

## Implementation contract

Sources: IEEE Std 802.15.4-2024, 10.20.1, 10.20.2.1,
10.20.3.1, 10.20.4.2/3, Table 10-100, 7.5.4 and Figure 7-30,
and 10.2.5 for realignment outside an orphan scan. These source nodes and
their normative continuations were retrieved before implementation.

An ordinary device's ORPHAN scan uses ordered channels and the existing unslotted
transaction engine. On each channel it sends command 6 with extended source,
broadcast short destination/PAN, compressed PAN and no ACK. It then listens for
macResponseWaitTime base-superframe periods; ScanDuration is ignored. Only
valid unsecured directed Coordinator Realignment frames addressed to its full
EUI are relevant. A frame completed at the deadline precedes the lower-priority
scan timeout. No beacon, DATA or unrelated command can finish the scan.

The coordinator reports Orphan Notification to its higher layer without
inventing a membership decision. The higher layer responds with the full orphan
identity, short address and AssociatedMember. False is the standard's ignored
response: the native exactly-one terminal carrier reports local IGNORED, emits
no frame and does not pretend to issue successful IEEE COMM-STATUS. True
constructs command 8 from the current authoritative PHY channel, PAN and local
short address; no stale MAC-side channel cache is introduced. It sends directly
through CSMA with AR=1, extended source/destination and an explicit source PAN.
The correlated native carrier reports actual transmission/ACK failure or success.
Selected eight-bit channels are 11–26; extended channel encodings remain unsupported.

A received valid directed realignment is acknowledged. On successful recovery,
the PHY accepts the advertised channel before PAN, short address, coordinator
references and AssociatedPanCoord are committed and published. The successful
scan retains this recovered configuration rather than restoring the old channel.
Failure, cancel and reset retain/restore the prior operating configuration;
NO_BEACON leaves the higher layer to choose reset/reassociation. Failure does not
fabricate a new PAN or remote disassociation. A failed channel commit restores
the saved channel. The state transition commits before observable publication,
so reentrant reset/stop cannot retain mismatched PHY and PIB configurations.

Outside an orphan scan, a realignment from the configured coordinator is ACKed
when requested and reported with REALIGNMENT through SYNC-LOSS. It does not
silently change the device's configuration (10.2.5). Thus a repeated realignment
after a lost ACK cannot reapply scan state. PAN reselection remains higher-layer
policy. Changing PAN with START clears stale MAC member references; the higher
layer can explicitly recognize and restore them through orphan responses.

The existing scan/PHY IDs, timers and local cancellation/reset/stop barriers own
all terminal paths. Proposed production evidence covers multiple-channel
recovery, full EUI discrimination, unknown member/no response, invalid or unrelated
frames, recovered PAN/channel/short identity, lost realignment ACK and scan
interruption. A native post-recovery DATA/ACK exchange verifies actual integration.
This contract was validated against the current scan, ACK, frame-exchange,
PHY GET/SET and lifecycle paths before edits.
