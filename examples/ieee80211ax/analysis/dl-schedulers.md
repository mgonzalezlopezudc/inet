# Downlink OFDMA schedulers

![Downlink scheduler dashboard](figures/dl/dl-scheduler-dashboard.png)

IEEE 802.11ax defines HE MU resource signaling and transmission procedures, but it does not mandate a single AP scheduling objective. Scheduler behavior must therefore be evaluated against an explicit workload rather than labeled globally “better.”

The top row is a symmetric three-flow comparison of SU EDCA, equal-RU first-backlogged-work, and equal-RU first-head-of-line policies. The bottom row isolates backlog-based versus minimum-HoL-delay scheduling under 80, 8, and 0.8 Mbit/s offered flows. Goodput and packet-delay p95 are computed once per run. Symmetric fairness uses station goodputs directly; asymmetric fairness first divides each station's goodput by its offered rate so a scheduler is not penalized for respecting unequal demand.

Expected behavior is workload dependent: equal-RU policies should be similar under symmetric saturation, backlog scheduling may favor the heavy flow, and HoL scheduling may improve delay service for older packets. Confidence intervals, not the ordering of one run, determine whether the observed differences are stable. The 0.55–0.88 s window excludes association and queue startup.
