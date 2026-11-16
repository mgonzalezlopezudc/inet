IEEE 802.15.4 GTS management contract
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Native management providers implement ``submitGts`` and clients implement
``handleGtsConfirm`` and ``handleGtsIndication``. Rebuild custom providers
and clients with these paired methods. The standard MAC accepts supported
allocation requests from devices tracking their associated PAN coordinator.
Confirmation follows the matching received beacon descriptor, or reports the
appropriate failure or retirement. Requests without that binding and explicit
deallocation requests are refused with ``UNSUPPORTED_OPERATION``. The DATA
request's default-false ``gtsTx`` field selects a matching GTS for direct DATA;
custom callers keep contention access unless they opt in. A missing matching
reservation returns ``INVALID_GTS`` and an exchange too large for its whole
reservation returns ``FRAME_TOO_LONG``.

Custom superframe providers and clients also implement the paired
``scheduleCfpBoundary``/``cancelCfpBoundary`` and ``handleCfpBoundary``
methods. The provider schedules the supplied future physical edge; the MAC
derives that edge from the actual beacon epoch and the selected clock.
