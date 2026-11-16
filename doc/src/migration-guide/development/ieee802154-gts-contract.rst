IEEE 802.15.4 GTS management contract
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Native management providers implement ``submitGts`` and clients implement
``handleGtsConfirm`` and ``handleGtsIndication``. Rebuild custom providers
and clients with these paired methods. The standard MAC accepts supported
allocation requests from devices tracking their associated PAN coordinator.
Confirmation follows the matching received beacon descriptor, or reports the
appropriate failure or retirement. Requests without that binding and explicit
deallocation requests are refused with ``UNSUPPORTED_OPERATION``. GTS DATA
transmission is not yet supported.
