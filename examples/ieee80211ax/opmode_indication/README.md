# HE Operating Mode Indication

This entry point demonstrates an Operating Mode Indication carried in the HE
variant HT Control field. One station advertises RX NSS 2 and sets UL MU
Disable, allowing the AP to update that peer's operating constraints without a
new association. RX NSS constrains what the station can receive; UL MU Disable
controls its participation in uplink MU operation. They are independent fields.

```sh
bin/inet -u Cmdenv -c OperatingModeIndication examples/ieee80211ax/mac_features/operating_mode_indication/omnetpp.ini
```

Verify the transmitted OM Control field and the AP's stored peer state before
and after it is received. Capability flags alone show only that the exchange is
allowed, not that the update occurred.
