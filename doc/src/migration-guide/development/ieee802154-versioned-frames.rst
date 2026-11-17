IEEE 802.15.4 versioned frame fields
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Rebuild custom users of ``Ieee802154Frame`` after adding the sequence
suppression and information-element-presence controls. Use
``Ieee802154FrameCodec::sequenceNumber``, ``destinationPanField`` and
``sourcePanField`` when the presence of a wire field matters: zero and
``0xffff`` are valid values, not absence markers. Structural decoding alone
does not enable an operational feature; the separate enhanced DATA option
selects the supported unsecured, unacknowledged native service.

``Ieee802154InformationElements::inspect`` computes bounded fixed-prefix,
auxiliary, IE, data and FCS regions from that frame's stored payload; it does
not own a second copy of the frame. Its returned ordered element values can be
used for inspection or explicit IE encoding. Protected payload and MIC stay
opaque, and a reserved security level remains unsupported operationally.
``inspectNested`` and ``encodeNested`` operate on short and long Nested IEs
inside an MLME Payload IE; the enclosing payload length remains the bound.
Unknown sub-IDs retain their form, order and contents without enabling their
feature-specific semantics.
