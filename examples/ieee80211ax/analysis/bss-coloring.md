# BSS coloring and OBSS/PD

![BSS coloring comparison](figures/bss/bss-coloring-comparison.png)

IEEE Std 802.11-2024 Clause 26.10 distinguishes OBSS/PD-based and parameterized spatial reuse (`80211ax-2024:chunk:09886`). The Spatial Reuse Parameter Set carries OBSS/PD bounds and BSS-color information (Clause 9.4.2.251, `80211ax-2024:chunk:03659`–`03661`). A more permissive threshold can enable concurrency, but it can also increase interference; same-color BSSs must not be treated as distinct OBSSs for this decision.

Both BSSs use the common `0.2–0.25 s` warm-up and start normal traffic at
`0.3 s`. The refreshed topology places the APs at `(200,250)` and `(400,250)`
meters, with each pair of STAs 30 m from its AP. Normal downlink traffic is
offered every 2 ms. The medium limit cache is raised to `100 ms` because this
load can create aggregated transmissions longer than its generic `10 ms`
default. Every AP and STA is assigned its BSS color; a zero local color would
disable OBSS/PD classification in the receiver.

The decisive third panel measures simultaneous AP transmission using
`transmissionState == 2`, the actual `TRANSMITTING` enum value. In the
refreshed five-seed results every condition is identical: `6.649 ± 0.165 Mbps`
aggregate goodput, `0.900 ± 0.081` Jain fairness, and `0.532 ± 0.258%`
concurrent AP airtime (95% Student-t confidence intervals). The vectors and
fresh two-AP PCAPs verify the traffic and capture points, but this scalar-medium
scenario still does not isolate an OBSS/PD throughput benefit. The result is a
model/workload limitation, not evidence that spatial reuse is ineffective in
general.
