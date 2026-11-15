# Pending broadcast release

> **Kind:** procedure · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [indirect.md](indirect.md)

`Ieee802154Broadcast.test` runs twelve scenarios with typed frames and raw bytes.
Two stationary production interfaces use PAN 0x1234. The coordinator queues DATA;
the device knows its full coordinator identity and initially listens through an
indefinite AutoOff receive task. An explicitly requested beacon consumes that
task, and its Frame Pending bit supplies the device's remaining receive demand.
No periodic beacon scheduler is used.

The cases cover a single queued broadcast, three FIFO broadcasts, extended
all-ones address and broadcast-PAN wire preservation, persistence expiry, PURGE,
queued reset, failed beacon access, failed release followed by a later beacon,
and cancellation/reset/stop during release CCA. A rewritten all-ones-source POLL
must return NO_DATA without releasing the broadcast queue before its beacon.
A subscriber inspects actual
outgoing MAC frames: broadcast addresses stay out of the pending-address list,
the beacon advertises pending broadcasts, DATA never requests ACK, DSNs remain
stable, and only DATA with another pending broadcast sets Frame Pending.

Receiver assertions check real MSDU bytes/order and return to OFF after the last
broadcast. Sender assertions check one terminal result per accepted DATA, with
no ACK metadata. Failed access retains the transaction until a later beacon;
success is emitted after actual transmission. Interruptions cover local pending
and CCA cleanup. They do not prove retraction of a waveform already emitted by
the shared packet-level medium.
