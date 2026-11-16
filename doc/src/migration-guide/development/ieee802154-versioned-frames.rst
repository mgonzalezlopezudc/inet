IEEE 802.15.4 versioned frame fields
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Rebuild custom users of ``Ieee802154Frame`` after adding the sequence
suppression and information-element-presence controls. Use
``Ieee802154FrameCodec::sequenceNumber``, ``destinationPanField`` and
``sourcePanField`` when the presence of a wire field matters: zero and
``0xffff`` are valid values, not absence markers. Version-2 structural
decoding does not enable enhanced MAC delivery or acknowledgement.
