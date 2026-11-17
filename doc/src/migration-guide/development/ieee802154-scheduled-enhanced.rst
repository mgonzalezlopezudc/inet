IEEE 802.15.4 scheduled enhanced frames
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Custom implementations of ``IIeee802154SuperframeProvider`` must update
``recordReceptionStart`` to accept the optional copied CAP window and bounded
CFP candidate values, including the original activity end and guarded ACK end.
The provider associates them with the existing PHY
transmission ID and returns them with the matching reception anchor. They are
transient values, cleared with the reception anchors, and do not replace the
authoritative schedule. A missing reception identity must not use another
frame's reservation.
When multiple receive grants overlap in their readiness intervals, preserve
all copied candidates. The MAC rechecks the candidate's PAN, device identities,
direction and live binding before using it. The original physical end bounds
qualifying DATA activity; the guarded end bounds ACK completion.

``Ieee802154PanDescriptor`` can carry optional simplified enhanced superframe
and GTS values. Custom management clients should check those optionals and
continue to treat legacy beacon content as unavailable when
``contentAvailable`` is false.
