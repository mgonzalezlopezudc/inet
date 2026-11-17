IEEE 802.15.4 enhanced acknowledgements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Rebuild custom native DATA clients for the appended optional enhanced ACK
content in ``Ieee802154MacDataConfirm``. It is absent when no enhanced ACK
was accepted and present for an actual ACK even if its IE lists and payload
are empty. Receiver response content is selected by the default-empty typed
``enhancedAckHeaderIes``, ``enhancedAckPayloadIes`` and ``enhancedAckPayload``
parameters; it is not a new standard PIB. Configured vendor identifiers must
contain three octets, and the encoded body is limited to 104 octets so the
largest supported MHR and FCS fit in the selected 127-octet PSDU.
