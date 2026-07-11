# HE Multi-TID Block Ack

The downlink and uplink entry points retain their respective traffic direction
and application-to-TID mappings.

```sh
bin/inet -u Cmdenv -c MultiTidBlockAck examples/ieee80211ax/mac_features/multi_tid_block_ack/downlink.ini
bin/inet -u Cmdenv -c MultiTidBlockAck examples/ieee80211ax/mac_features/multi_tid_block_ack/uplink.ini
```

The current model negotiates Multi-TID capability and exercises BAR/Block Ack
plumbing; these simulations are not proof of full aggregated multi-TID
interoperability.
