# HE MU-MIMO

These focused entry points run the HE downlink and uplink MU-MIMO scenarios.
They include the existing DL/UL topology configurations so module paths,
traffic, seeds, and effective parameter precedence remain unchanged.

```sh
bin/inet -u Cmdenv -c DlMuMimo examples/ieee80211ax/multi_user/mu_mimo/downlink.ini
bin/inet -u Cmdenv -c DlMuMimo80MHz examples/ieee80211ax/multi_user/mu_mimo/downlink.ini
bin/inet -u Cmdenv -c UlMuMimo examples/ieee80211ax/multi_user/mu_mimo/uplink.ini
```

The canonical parameter blocks currently remain in `dl_ofdma/omnetpp.ini` and
`ul_ofdma/omnetpp.ini` to keep their established entry points compatible.
