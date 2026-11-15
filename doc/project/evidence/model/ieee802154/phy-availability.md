# Native PHY availability and lifecycle recovery

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [phy-services.md](phy-services.md), [data-exchange.md](data-exchange.md)

Independent lifecycle review found that a PHY-only stop could leave the MAC's
cached RECEIVE state unchanged. The MAC collapsed STOPPED into an IEEE access
failure and later skipped receive readiness forever. An idle stop/restart had
no pending PHY request from which to obtain a corrective callback.

The native PHY owner will issue a direct availability indication with actual
availability and transceiver state after lifecycle changes. MAC local lifecycle
and PHY availability are distinct: a down PHY does not stop the MAC or erase
its PIB/queued frames. The MAC caches the declared dependency availability,
refuses new radio-dependent requests while down and restores idle receive policy
when it returns. Local attribute access and queued indirect admission remain
available. Normal PHY state/measurement/transmit timing does not change.

The provider detaches its interrupted operation/reset identity and destroys its
owned packet before notifications. Old aborts use those snapshots, so callback
restart cannot cancel new work. The MAC atomically retires radio-dependent
operations with local STOPPED, clears old extraction grants and releases held
ACK processing without claiming an ACK was sent. Its existing peer-cleanup
admission barrier becomes a general synchronous retirement barrier, preserving
an outer barrier during nested callbacks. Availability-up can be observed during
callbacks; physical continuation waits until retirement finishes. No generation,
polling timer or replacement state machine is introduced.

Re-review found a nested reset ordering case: stopping only the PHY from an
active DATA's RESET callback could confirm RESET before queued DATA was retired.
The reset request therefore retains a deferred terminal result while its whole
synchronous retirement phase runs, including management and direct DATA callbacks.
It releases that result only after queued callbacks finish. This extends the
existing queue callback barrier to the reset's complete retirement boundary.
The focused fixture adds direct-plus-queued DATA and callback stop/restart cases;
both DATA terminals must precede the one reset terminal, even after restart.

Already installed receiver schedules retain their documented activity/configuration
lifetime; finite expiry remains a logical deadline. A PHY-only unavailable scan
cannot restore a channel through an unavailable provider. It completes STOPPED
without fictitious restoration; the actual PHY channel remains observable after
restart. Normal cancel/reset/whole-interface shutdown restoration retains its
existing evidence. Already emitted waveforms retain the existing medium limit.

Focused production cases stop only the PHY during CCA, TX readiness,
on-air TX, ACK waiting and idle/backoff. They check local abort identity, packet
destruction, refusal while down, automatic idle RX restoration and actual DATA
after restart. A callback-restart case exercises snapshot retirement directly.
Existing structural clients gain an explicit no-op availability handler; native
paired contracts remain pure roles. Scope/source protection and the provider's
inline callbacks were validated before this repair.

The 20 typed/raw cases passed, including nested reset ordering. The same
independent reviewer confirmed all lifecycle and configuration findings resolved:
24 PASS, 10 N/A, zero FLAG/QUESTION in the scoped canonical checklist. The review
excludes unrelated codec/application correctness and whole-standard conformance.
Crash uses the inspected common stop handler; this focused fixture does not
separately execute PHY-only crash.

The same review's configuration correction supplies sibling-binding defaults
and role icons. A protocol-local NED function supplies a canonical locally
administered 64-bit default address from the simulator component ID. Explicit
configured EUI values keep their exact meaning; no auto sentinel, 48-bit mapping,
random draw or global address counter is added. The default-composition fixture
will check distinct full native identities and their declared owner context.
