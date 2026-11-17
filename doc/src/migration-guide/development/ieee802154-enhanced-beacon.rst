IEEE 802.15.4 enhanced beacon descriptors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``Ieee802154PanDescriptor::coordinatorPanId`` and ``sequenceNumber`` are now
optional. Update custom native management clients to test presence before
using either value. Legacy beacons keep both values present; an enhanced
beacon can omit them. ``sourcePanId`` reports the actual wire field, while
``coordinatorPanId`` is an effective value only when a valid context exists.
``Ieee802154MacBeaconRequestIndication::destinationPanId`` is also optional:
request clients must check whether the field was carried instead of treating
an absent field as a literal or fallback PAN value.
The copied enhanced Header/Payload IEs are separate from the legacy beacon
body, whose ``contentAvailable`` flag remains false for enhanced beacons.

The enhanced profile also exposes its own EBSN and beacon IE configuration
through the existing MAC PIB. Active enhanced scans and one-shot enhanced
beacons remain separate from periodic enhanced superframe operation.
``Ieee802154MacPibValue`` also has typed Header-IE and Payload-IE vector
alternatives. Custom visitors that assume an exhaustive set of alternatives
must handle these MAC values; they are not PHY PIB values.
Native enhanced active scans include the local extended address in the request.
An upper client responding manually should copy the indicated requester address
into the response destination. A Source PAN omitted on wire remains absent in
``sourcePanId``, even when the effective coordinator PAN is inferred from the
carried Destination PAN.
