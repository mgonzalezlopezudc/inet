IEEE 802.15.4 Superframe Management
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``IIeee802154ManagementServiceProvider`` adds ``submitStartSuperframe()`` and
``IIeee802154ManagementServiceClient`` adds ``handleStartSuperframeConfirm()``. Custom providers
and clients must implement these methods and rebuild. The provider and client use the request and
confirmation types in ``Ieee802154MacStartSuperframeRequest.h``; a client that does not request
this service may treat its confirmation as unexpected. Local admission and terminal confirmation
remain separate, and an accepted request receives one terminal confirmation.

Custom ``IIeee802154SuperframeClient`` implementations must additionally implement
``handleSuperframeCapEnd()``. Custom providers must implement the clock-target preparation,
conversion and CAP-end methods in ``IIeee802154SuperframeProvider``. Preparation is reported
before the target by the supplied physical readiness lead; it is not a transmission-start event.
