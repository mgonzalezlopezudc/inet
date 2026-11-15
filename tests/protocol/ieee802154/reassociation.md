# Repeated association episodes

> **Kind:** procedure · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [association.md](association.md)

`Ieee802154Reassociation.test` runs four scenarios with typed frames and raw
bytes. Two stationary production interfaces communicate over the scalar medium.
The coordinator starts PAN 0x1234; the device resets, actively scans, joins and
sends acknowledged DATA. A second episode deliberately reuses wire DSN 7 while
allocating a different short address. This checks episode isolation without
assuming duplicate suppression or treating an ACK as admission.

The scenarios cover leave and rejoin, canceled association followed by reset
and rejoin, a late same-PAN Association Response after the second join, and a
second PAN followed by a response naming the old PAN. A TAP replaces a real DATA
PPDU with an equal-length, valid-FCS Association Response for the late-input
cases. The same-PAN response can be acknowledged but cannot change short address
0x43 or create another association confirmation. Foreign-PAN input is rejected.

Assertions inspect current PIB references, actual DATA/ACK delivery and exactly
one terminal callback for each accepted request. The fixture does not model a
higher-layer allocator or prove duplicate handling for secured frames.
