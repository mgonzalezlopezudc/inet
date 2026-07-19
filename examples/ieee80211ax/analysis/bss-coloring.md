# BSS coloring and OBSS/PD

![BSS coloring comparison](figures/bss/bss-coloring-comparison.png)

IEEE Std 802.11-2024 Clause 26.10 distinguishes OBSS/PD-based and parameterized spatial reuse (`80211ax-2024:chunk:09886`). The Spatial Reuse Parameter Set carries OBSS/PD bounds and BSS-color information (Clause 9.4.2.251, `80211ax-2024:chunk:03659`–`03661`). A more permissive threshold can enable concurrency, but it can also increase interference; same-color BSSs must not be treated as distinct OBSSs for this decision.

Both BSSs use the common `0.2–0.25 s` warm-up and start normal traffic at
`0.3 s`. The refreshed topology places the APs at `(200,250)` and `(400,250)`
meters, with each pair of STAs 30 m from its AP. Normal downlink traffic uses
independent intervals uniformly distributed between `1 ms` and `1.4 ms`. This
keeps both APs backlogged without synchronizing all four application streams.
The medium limit cache is raised to `100 ms` because this load can create
aggregated transmissions longer than its generic `10 ms` default. Every AP and
STA is assigned its BSS color; a zero local color would disable OBSS/PD
classification in the receiver.

The decisive third panel measures simultaneous AP transmission using
`transmissionState == 2`, the actual `TRANSMITTING` enum value. In the
refreshed five-seed results, the disabled, `-82 dBm` conservative, and
same-color collision conditions produce `6.540 ± 0.338 Mbps`, `0.965 ± 0.016`
Jain fairness, and `0.444 ± 0.178%` concurrent AP airtime. The `-79 dBm`
enabled and `-62 dBm` aggressive conditions produce `8.446 ± 0.808 Mbps`,
`0.922 ± 0.154` fairness, and `15.416 ± 3.034%` concurrent airtime (95%
Student-t confidence intervals). Thus correct coloring and OBSS/PD increase
aggregate goodput by about 29%; the same-color control proves that merely
enabling the receiver option is insufficient.
