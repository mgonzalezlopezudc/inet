# HE MU-MIMO

MU-MIMO serves several stations on overlapping frequency resources by assigning
spatial streams, unlike OFDMA, which separates users into RUs. These focused
entry points run the HE downlink and uplink MU-MIMO scenarios while preserving
the canonical DL/UL topology, traffic, seeds, and parameter precedence.

```sh
bin/inet -u Cmdenv -c DlMuMimo examples/ieee80211ax/multi_user/mu_mimo/downlink.ini
bin/inet -u Cmdenv -c DlMuMimo80MHz examples/ieee80211ax/multi_user/mu_mimo/downlink.ini
bin/inet -u Cmdenv -c UlMuMimo examples/ieee80211ax/multi_user/mu_mimo/uplink.ini
```

The canonical parameter blocks currently remain in `dl_ofdma/omnetpp.ini` and
`ul_ofdma/omnetpp.ini` to keep their established entry points compatible.

In the downlink case the AP is the beamformer: it performs NDP sounding,
collects channel-state feedback, and schedules eligible beamformees. In the
uplink case the AP Trigger frame aligns HE TB transmissions and assigns spatial
streams on a full-bandwidth RU. Antenna counts and capability flags establish
model eligibility; the actual MU PPDU, user NSS, and CSI table are the evidence
that spatial multiplexing occurred.

In Qtenv inspect `ap.wlan[0].mac.hcf.csiManager.csiTable`, the scheduler's
`lastScheduleSummary`, and the radio transmitter's
`lastHeUserPhyParameters`. Compare against the corresponding OFDMA or SU
configuration before interpreting packet counts.
