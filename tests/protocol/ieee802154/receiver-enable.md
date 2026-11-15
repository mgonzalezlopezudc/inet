# Scheduled receiver control

> **Kind:** procedure · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [../../../doc/project/evidence/model/ieee802154/receiver-enable.md](../../../doc/project/evidence/model/ieee802154/receiver-enable.md)

`Ieee802154ReceiverEnable.test` uses two production standard interfaces over a
scalar medium. The sender remains receive-ready; the receiver normally sleeps.
The native management client installs schedules at 1 ms and sends real DATA
probes. All 38 scenarios run with typed frames and raw bytes. Assertions compare
packet delivery, actual radio mode, expiry timestamps, terminal counts and PIB
retention, rather than inferring reception from a successful sender confirmation.

Cases cover ASAP and absolute finite windows, one-symbol and maximum-finite
duration, indefinite receive and explicit disable, AutoOff with and without a
frame, foreign-destination AutoOff, replacement, cancellation by an empty list,
invalid widths/list size, unsupported ranging, half-counter and rollover edges,
past/deferred timestamps, and a locally refused impossible readiness deadline.
The clock is an ideal 24-bit 16-us-symbol clock; long-time cases jump through
268 seconds without generating artificial intermediate events.

Primary-duty cases send a 100-byte acknowledged DATA while receive demand is
active, expire the demand during that transmission, extract an actual indirect
DATA through POLL after AutoOff consumes its ACK, and keep passive scanning
receive-ready across scheduled expiry. Stop/reset and confirmation/indication
reentry check cancellation without late or duplicate callbacks. RxOnWhenIdle
also keeps the physical receiver enabled after the secondary task expires.

Three additional cases complete an incoming DATA during a 2-ms CCA. The selected
6.3.2.1 discard policy gives no receive indication or ACK. With no sender retries,
AR traffic reports NO_ACK; no-AR transmission reports SUCCESS without inventing
delivery. The busy CCA still drives the local request's backoff/failure boundary.
These cases exercise the permitted discard policy, not concurrent ACK preemption.
