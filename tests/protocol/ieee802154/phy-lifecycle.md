# Native PHY dependency recovery

> **Kind:** procedure · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [lifecycle.md](lifecycle.md)

`Ieee802154PhyLifecycle.test` connects two production native interfaces at one
metre, PAN 0x1234, scalar O-QPSK radio and -110 dBm background noise. Only the
sender's PHY receives a module STOP; its MAC remains active. Eight scenarios
cross typed/raw representations: CCA, idle receive, TX readiness, emitted TX,
ACK waiting, initial RX readiness, random backoff and restart from the interrupted
DATA confirmation. The callback restart runs inside synchronous retirement.

The original accepted MSDU must be destroyed before exactly one native STOPPED
confirmation. DATA admission during retirement is BUSY; after stopping it is
UNAVAILABLE and retains caller ownership. Idle stop has no original confirmation.
Restart restores actual receiver mode when RxOnWhenIdle is true. A replacement
MSDU must then complete SUCCESS through actual DATA and immediate ACK. No test
pretends to retract a waveform already emitted into the packet medium.

The protocol observer requires replacementDone and the clients independently
assert result identities, packet destruction, callback counts and radio mode.
Two additional scenarios queue an indirect MSDU behind active direct DATA, then
RESET. The direct DATA's RESET callback stops the PHY; the second scenario also
restarts it inline. Both DATA terminals must precede exactly one reset STOPPED
confirmation. All 20 typed/raw runs passed in debug after the availability and
reset-order repairs. This fixture executes STOP/START; crash shares the inspected
stop handler but is not separately executed here. Run from the checkout:
`opp_run_protocol_tests --load . -p inet -m debug --no-build -w '^tests/protocol/ieee802154$' -f 'Ieee802154PhyLifecycle\.test$'`.
