# Bidirectional downlink and uplink OFDMA

This example compares simultaneous downlink and uplink traffic in two matched
IEEE 802.11ax configurations:

- `Ofdma` uses equal-sized downlink resource units and backlog-based scheduled
  uplink resource units.
- `Su` disables both multi-user paths while retaining the same `HeHcf` queue
  organization, PHY settings, traffic, topology, and random seed.

Three nearby stations exchange best-effort UDP traffic with the wired server.
The downlink offers 2.4 Mbps and the continuously active uplink offers
0.48 Mbps. Small packets expose the contention and per-PPDU overhead that
OFDMA amortizes across users. A 50 ms minimum uplink Trigger interval batches
uplink backlog without allowing Trigger exchanges to consume every AP
transmission opportunity.

Run both configurations from the INET project root:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/dl_ul_ofdma/omnetpp.ini -c Ofdma -r 0
bin/inet -u Cmdenv -f examples/ieee80211ax/dl_ul_ofdma/omnetpp.ini -c Su -r 0
```

The INI defines five repetitions (`-r 0..4`) for a paired comparison across
independent seed sets.

Compare received bytes at `host[*].app[0]` for downlink delivery and at
`server.app[0]` for uplink delivery. The OFDMA run should also contain
downlink HE MU allocations and scheduled HE-TB uplink responses; the SU
control should contain neither.
