# IEEE 802.15.4 slotted CAP access

Plan: [B2-02](../../../plan/pending/802154-superframe-gts-plan.md#b2-02--ieee802154-add-slotted-cap-channel-access),
`ieee802154: add: slotted CAP channel access`. The primary fixture is
[Ieee802154SlottedAccess.test](Ieee802154SlottedAccess.test); the companion fixtures are
listed below.

The coordinator and children use the native StandardInterface, PHY and packet-level
O-QPSK radio. The access provider draws backoff periods and requests actual PHY CCAs;
the MAC owns the DATA packet and the PHY transmit request. The contention fixture uses
BO=3/SO=1, two and five contenders, BLE on/off, twenty seeds and typed/raw radio
serialization. It observes physical transmissions and CAP boundaries, CW transitions,
one DATA terminal per accepted request and received packets. Its finite-horizon
cancellations do not imply fairness or that every transmitted packet was received.

Other fixtures use two native peers or a native child and a separately controlled real
PHY/radio. They exercise first/second busy CCA, BLE countdown, an inbound DATA/ACK/IFS
interrupting access, a 127-byte PSDU with insufficient full versus remaining CAP,
physical ACK and CAP end boundaries, and an ACK lost once or on every attempt. The
search cases turn off a real coordinator after a beacon: an unsent DATA searches after
four misses before fallback; an already-transmitted CAP retry cannot fall back and
resumes with the same DSN when a real parent beacon returns. Separate cases retire an
expired CAP, replace SYNC on a new channel during an open CAP, and shut down beaconing
with BO15 while DATA is submitted synchronously from the confirmation. Lifecycle cases
stop or reset at access phases and submit fresh DATA from RESET.confirm. The priority
fixture observes an actual coordinator beacon after a deferred DATA and tests ACK
timeout equality with the scheduled preparation event on simulation time and both
oscillator event backends. No test substitutes a helper result for the radio path.

The expired-CAP search control explicitly enables promiscuous receive duty on
the searching device. Its restarted, intentionally nonbeaconing peer sets
BO15, so its configured idle receive duty can accept fallback DATA. The search
deadline and actual transmission assertions remain unchanged.

The assumed parent has beacon timing and unslotted access but refuses beacon-mode DATA;
it cannot produce these two-CCA, slotted PPDU and cross-CAP observations. This is a
source-based parent expectation, not an executed parent comparison.

The two deferrals have different state:

```text
CAP A                     inactive                   CAP B
| countdown ... | residual periods retained | ... countdown -> CCA
| countdown ends | exchange does not fit    | new draw -> CCA
```

With BLE enabled, countdown and the start of DATA are confined to the first configured
full backoff periods after beacon IFS. The DATA exchange still has to fit the geometric
CAP; a count or exchange that cannot proceed in the BLE window draws again next CAP.

```text
beacon | IFS | BLE full-period window | remaining geometric CAP
              countdown and TX start      exchange may finish here
```

The period is measured on the local clock. CCA duration, radio turnaround, propagation
and airtime remain physical durations. A coordinator reserves its actual next beacon
preparation time separately from the geometric CAP; the final IFS need not occupy its
radio. The full-CAP FRAME_TOO_LONG test uses the beacon-end to CAP-end duration and
the DATA/ACK/IFS exchange, without CCA or the local preparation reservation.

The external-clock profile requires fixed clock rates within ±40 ppm, a nominal
clock quantum dividing 16 microseconds, a grid-aligned initial offset and no clock
steps or rate changes. The current OscillatorBasedClock uses `clockGranularity=0`
for this profile. The no-clock configuration uses simulation time, which must
represent a 16-microsecond symbol.

The first full contention run attempted all 160 cases: 150 passed and ten had a request
pending at the 1.23-second assertion. In the traced seed, that provider drew again
after each recent beacon, including 100 microseconds before the cutoff. The revised
fixed-horizon test checks that progress, cancels requests still pending at the cutoff,
and observes subsequent CAPs for stale DATA starts. It passed all 160 runs. Across
those runs the per-run counts total 550 SUCCESS, zero CHANNEL_ACCESS_FAILURE, ten
CANCELLED and 338 received DATA indications; typed/raw repetitions are included in
each total. Raw results are in `/tmp/ieee802154-b202-matrix2-protocol.log` and the
retained execution artifacts. The three residual/redraw provider cases and five
size/CCA-handback cases also passed (`/tmp/ieee802154-b202-matrix2-module.log`).

The earlier two-node, zero-BE smoke passed 40 typed/raw seed repetitions; it is not
evidence for the contention campaign. Additional tests in this package are:

| Fixture | Direct observation | Last completed result |
| --- | --- | --- |
| `Ieee802154SlottedClock.test` | Real DATA start against the receiver-clock grid and captured physical CAP end, both ±40 ppm signs and four rounding modes | 16/16 passed |
| `Ieee802154SlottedSearch.test` | Actual RX readiness, fallback, tracking/acquisition loss and recovery, expired CAP, replacement SYNC and BO15 shutdown callback reentry | 20/20 passed |
| `Ieee802154SlottedLifecycle.test` | Reset/stop at access phases, RESET-confirm fresh-DATA reentry, and idle provider stop/restart | 26/26 passed |
| `Ieee802154SlottedRetry.test` | Dropped real ACKs, retained DSN and retry in a later CAP | 6/6 passed |
| `Ieee802154SlottedLateAck.test` | Scripted PHY sends a permitted later-grid ACK after the old wait bound, plus physical timeout-end and one-tick-late probes | 12/12 passed |
| `Ieee802154SlottedFit.test` | Real DATA and scripted ACK/IFS ending one tick before, exactly at, or one tick after the physical CAP boundary | 6/6 passed |
| `Ieee802154SlottedTooLong.test` | Independently encoded 127-byte DATA in valid final-CAP-slot 7 versus 15 windows with a 1 ms declared propagation bound, one FRAME_TOO_LONG or cancellation and original-packet destruction | 4/4 passed |
| `Ieee802154SlottedBusy.test` | Timed real PHY energy during the first or second CCA, both BLE settings, NB/CW reset and exhaustion | 12/12 passed |
| `Ieee802154SlottedPriority.test` | Actual next coordinator beacon precedes resumed DATA; ACK timeout equality uses an independent physical preparation probe on three clock backends | 12/12 passed |
| `Ieee802154SlottedIfs.test` | Actual incoming DATA, ACK and IFS postpone a pending CCA; the same draw resumes on the beacon grid | 2/2 passed |

Clock results are in `/tmp/ieee802154-b202-clock-search-protocol.log`; late ACK and
lifecycle reentry are in `/tmp/ieee802154-b202-lateack-lifecycle.log`; physical CAP
fit is in `/tmp/ieee802154-b202-search-fit.log`; and the current busy, valid-CAP,
expired-window and beacon-priority results are in `/tmp/ieee802154-b202-busy-priority.log`.
The first combined checkpoint passed three unit, seven module and 22 protocol files,
638 simulations in total, before the added pre-acquisition DATA case. After that fix,
the matching combined rerun passed the same selected files with 640/640 simulations,
including 276 slotted cases, and no protocol stderr. Its unit, module and protocol
logs are `/tmp/ieee802154-b202-final-unit.log`, `/tmp/ieee802154-b202-final-module.log`
and `/tmp/ieee802154-b202-final-protocol.log`; the raw runs, generated fixtures and
source/library identities are retained under
`/home/miguel/workspace/.codex-artifacts/ieee802154-followup-execution-20261116/b202-final-acceptance/`.
Independent source review remains in progress. Mermaid rendering is not applicable
to these text diagrams.

The retry fixture enables native PCAP recorders at each MAC. Its captured DATA and ACK
bytes can be independently checked with `tests/validation/ieee802154/check_slotted_capture.py`.
For each child capture, pass `--attempts 1`, `2` or `3` for no ACK loss, first ACK loss,
or loss of all ACKs, and pass `--expect-ack` for the first two cases. The checker uses
Scapy to validate the FCS, short addresses, PAN, payload and unchanged DSN. A capture's
send timestamp is the MAC's packet-to-lower signal, which may precede an already-scheduled
radio start; the radio observations above establish physical grid and CAP timing. The
Scapy 2.8.0 independently checked all six child captures: the no-loss cases contain
one DATA, the first-loss cases contain two and the all-loss cases contain three; DATA
DSNs agree within each capture and observed ACK DSNs match. The retained captures,
checker and per-run results are in
`/home/miguel/workspace/.codex-artifacts/ieee802154-followup-execution-20261116/b202-retry-capture/`.
TShark was unavailable and was not run.

From the repository root, the scoped protocol selection is:

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/ieee802154$' -f 'Ieee802154Slotted.*[.]test$'
```

The controller, unslotted access, copied-window and slotted role module fixtures and
the exchange-timing unit fixture cover the paired interfaces separately. The current
plan and root execution record specify the complete combined control selection.
