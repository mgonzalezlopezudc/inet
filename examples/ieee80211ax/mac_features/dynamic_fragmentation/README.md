# HE dynamic fragmentation

This entry point runs negotiated HE level-1 dynamic fragmentation using the
same topology and traffic definition as the uplink example. HE dynamic
fragmentation lets a transmitter choose fragment boundaries for eligible MPDUs
to fit the available transmission opportunity; it is distinct from IP
fragmentation and from A-MPDU aggregation.

```sh
bin/inet -u Cmdenv -c DynamicFragmentation examples/ieee80211ax/mac_features/dynamic_fragmentation/omnetpp.ini
```

Confirm both negotiated capability and fragment creation/reassembly counters.
Packet delivery alone cannot distinguish fragmentation from an unfragmented
fallback path.
