# HE NDP feedback report

This entry point isolates the NDP Feedback Report Poll (NFRP) exchange from the
broader uplink OFDMA example. An NFRP Trigger allocates feedback resources;
selected stations answer with HE TB NDP feedback rather than ordinary queued
MAC data. The exchange gives the AP compact per-station information without a
normal data PPDU.

```sh
bin/inet -u Cmdenv -c NdpFeedbackReport examples/ieee80211ax/multi_user/ndp_feedback/omnetpp.ini
```

Inspect the AP Trigger-frame counters and station feedback-response counters or
the corresponding Qtenv HCF watches. Do not infer a feedback response merely
from successful UDP delivery: the control exchange is the feature under test.
