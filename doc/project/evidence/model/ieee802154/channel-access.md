# IEEE 802.15.4 — unslotted access

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [phy-provider.md](phy-provider.md), [pib.md](pib.md)

Step 5's pre-write contract assigns random backoff, NB/BE and the backoff timer to
`Ieee802154UnslottedAccess`. The paired access provider/client roles keep the MAC exchange
owner as the PHY's single client: access requests a CCA from its MAC client, which performs
the real PHY service and returns the result. Access does not own packet retries, DSNs or radio
turnaround. A request carries immutable validated PIB inputs and a nonzero increasing ID.

IEEE Std 802.15.4-2024, 6.3.2.1 and Figure 6-2 (physical PDF pp. 63–64) initializes NB=0
and BE=macMinBe, draws uniformly in [0,2^BE-1] backoff periods, performs one CCA, and grants
access on idle. Busy increments NB, caps BE at macMaxBe and fails exactly when NB exceeds
macMaxCsmaBackoffs. Table 8-31's NumBackoffs uses this NB definition. A zero backoff calls
the CCA client inline without an artificial procedure event. A positive backoff has one timer.
The named `backoffRng` parameter selects the simulation RNG. Draw and state signals make the
chosen slots/NB/BE observable without using them as a second state owner.

Cancellation/reset/stop detach active state and cancel the timer before a terminal callback.
Old CCA results cannot complete a replacement request. Refusal transfers no ownership, and
accepted work copies its values. A disappeared client triggers timer cleanup without callback.
One terminal result distinguishes success, channel access failure and local aborts. Lifecycle
restart does not reset the last attempted ID.

Proposed direct fixture `tests/module/Ieee802154UnslottedAccess_1.test` checks zero-backoff,
idle, continuously busy, busy-then-idle, exponent/attempt limits, stale results and cancellation.
Production PHY/MAC integration and protocol observation are assigned to
`tests/protocol/ieee802154/Ieee802154UnslottedAccess.test` and `unslotted-access.md` in planned series
`IEEE802154-5` (`ieee802154: add: implement unslotted channel access`). Before integration no
replacement DATA transmission exists. The protocol fixture must show actual transmission
after successful CCA and none after access failure; the algorithm fixture alone cannot prove it.
