# Non-beacon discovery beacon generation

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [managed-applicability.md](managed-applicability.md), [scanning.md](scanning.md)

## Implementation contract

Validated before implementation against the current single transaction engine,
PIB BSN owner and command/beacon codecs. Sources: IEEE Std 802.15.4-2024,
7.3.1.2/4–7, 7.5.2, 8.2.10.1–4, Tables 8-22/23/25,
10.19 and the continuation after Figure 10-147.

The optional native management provider adds an unsecured legacy BEACON service
with a typed channel and superframe order. Unsupported enhanced/periodic options
return INVALID_PARAMETER, nonzero security requests report unavailable security,
and oversized generated MPDUs report FRAME_TOO_LONG. The PHY accepts the selected
channel before transmission starts. The MAC owns one independent BSN, chooses
short source when allocated and extended source for 0xfffe, and sends through
the same real unslotted CSMA/PHY engine as DATA without an ACK request.
Success follows actual PPDU completion. Cancellation/reset/stop use the engine's
existing ownership and local abort contract; emitted waveforms retain its known
abort limitation. Frame construction draws no RNG.

Superframe fields advertise BO/SO 15, current coordinator/association-permit bits,
no GTS and opaque PIB payload. Pending addresses come from live same-PAN store
entries, unique by full native identity, short before extended, at most seven.
No broadcast-pending support is implied by the address list.

A valid unsecured legacy Beacon Request uses broadcast PAN/short destination,
no source, no ACK and command ID 7. A non-beacon coordinator queues one beacon
response per accepted request for unslotted transmission. AutoRespond=false
instead emits BEACON-REQUEST indication; higher-layer policy may submit BEACON.
The queue is a count of identical response obligations, cleared on reset/stop.
It cannot create a new external request ID or consume the client's ID sequence.

Active scan will use the same transaction engine for a Beacon Request on each
channel. Dwell starts after actual request transmission and receive readiness.
Channel access failures enter UnscannedChannels. At least one sent request with
no received beacon yields NO_BEACON, including AutoRequest=false. Cancellation
retires the internal transaction before restoring scan configuration.
Proposed production tests cover actual discovery, AutoRespond=false, on-demand
beacons, BSN progression independent of DSN, failure statuses and scan restoration.
