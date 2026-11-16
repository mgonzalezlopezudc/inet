IEEE 802.15.4 association parent role
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``Ieee802154MacAssociateRequest`` now appends
``coordinatorIsPanCoordinator``. Its default is ``true``, preserving the prior
caller declaration for a configured PAN coordinator. A caller associating
through an ordinary coordinator sets it to ``false``. A discovery policy can
use the PAN-coordinator bit from the selected beacon; a matching acquired
beacon supplies the actual role when beacon tracking is used. Rebuild native
clients after this value type changes. The declaration is local context, not a
field carried by an Association Request or Response command.

``macAssociatedPanCoord`` can be false when the device is associated through
an ordinary coordinator. Clients that need to know whether association
succeeded should track the association completion instead of treating that PIB
value as a generic binding flag. Setting a PAN or coordinator address alone
does not establish association. A successful public SET that changes an
existing parent reference invalidates the binding, tracker, and linked
outgoing schedule before the SET confirmation.
