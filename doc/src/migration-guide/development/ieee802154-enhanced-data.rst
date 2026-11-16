IEEE 802.15.4 enhanced DATA
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Native DATA requests default to legacy transmission. Applications may opt in
to version-2 DATA through the copied request or packet tag and explicitly
enable enhanced frames on the interface. The selected profile supports
unsecured, unacknowledged DATA; unsupported layouts and IE features return a
native terminal status. An absent PAN field is resolved using an existing
association context, while received metadata preserves whether a source PAN
was actually carried on the wire. The native indication supplies copied Header
and Payload IE lists separately; the delivered packet contains the MSDU only.
Clients should rebuild for the appended request and indication fields.
