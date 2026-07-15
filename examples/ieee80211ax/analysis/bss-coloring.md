# BSS coloring and OBSS/PD

![BSS coloring comparison](figures/bss/bss-coloring-comparison.png)

IEEE Std 802.11-2024 Clause 26.10 distinguishes OBSS/PD-based and parameterized spatial reuse (`80211ax-2024:chunk:09886`). The Spatial Reuse Parameter Set carries OBSS/PD bounds and BSS-color information (Clause 9.4.2.251, `80211ax-2024:chunk:03659`–`03661`). A more permissive threshold can enable concurrency, but it can also increase interference; same-color BSSs must not be treated as distinct OBSSs for this decision.

Both BSSs use the common `0.2–0.25 s` warm-up and start normal traffic at
`0.3 s`. Disabled and color-collision controls are compared with conservative,
nominal, and aggressive OBSS/PD thresholds. The offered load is bounded to
keep HE PPDU durations valid, so aggregate goodput can remain load-limited even
when spatial reuse changes.

The decisive third panel measures simultaneous AP transmission using
`transmissionState == 2`, the actual `TRANSMITTING` enum value. In the
refreshed five-seed results every condition is identical: `6.4 Mbps` goodput,
`49.8%` concurrent AP airtime, and Jain fairness `1.0`. The vectors and fresh
two-AP PCAPs therefore verify the traffic and capture points, but this bounded
scenario does not isolate OBSS/PD behavior. The documentation deliberately
does not infer a spatial-reuse benefit from these results.
