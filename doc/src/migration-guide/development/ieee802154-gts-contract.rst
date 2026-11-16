IEEE 802.15.4 GTS management contract
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Native management providers implement ``submitGts`` and clients implement
``handleGtsConfirm`` and ``handleGtsIndication``. Rebuild custom providers
and clients with these paired methods. The standard MAC currently refuses GTS
requests with ``UNSUPPORTED_OPERATION`` after shared ID admission; no
confirmation or allocation exchange follows that refusal.
