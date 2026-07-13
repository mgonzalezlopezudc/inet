# BSS coloring and OBSS/PD

![BSS coloring comparison](figures/bss/bss-coloring-comparison.png)

IEEE Std 802.11-2024 Clause 26.10 distinguishes OBSS/PD-based and parameterized spatial reuse (`80211ax-2024:chunk:09886`). The Spatial Reuse Parameter Set carries OBSS/PD bounds and BSS-color information (Clause 9.4.2.251, `80211ax-2024:chunk:03659`–`03661`). A more permissive threshold can enable concurrency, but it can also increase interference; same-color BSSs must not be treated as distinct OBSSs for this decision.

Two overlapping BSSs are deliberately offset by 0.5 ms so identical application schedules cannot force artificial simultaneous starts. Disabled and color-collision controls are compared with conservative, nominal, and aggressive OBSS/PD thresholds. The offered load is bounded to keep HE PPDU durations valid, so aggregate goodput can remain load-limited even when spatial reuse changes.

The decisive third panel measures simultaneous AP transmission using `transmissionState == 2`, the actual `TRANSMITTING` enum value. The old `state != 0` test incorrectly counted idle state as airtime. Expect disabled, conservative, and same-color cases to serialize more, while permissive distinct-color cases show more overlap. Throughput and fairness reveal whether that concurrency is useful rather than assuming it is always beneficial.
