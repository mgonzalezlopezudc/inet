# Downlink scheduler plots

The dashboard compares aggregate goodput, 95th-percentile end-to-end delay, and
Jain fairness across EDCA, bandwidth-oriented, head-of-line, and backlog-aware
schedulers. `fBW` should favor capacity, while head-of-line policies should
reduce tail delay. Backlog-aware scheduling should respond to asymmetric
offered loads without starving smaller flows.

Every condition contributes one aggregate observation per run. Packet samples
are used only to calculate that run's delay percentile.
