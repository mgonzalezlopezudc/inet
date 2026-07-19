# BSS coloring and OBSS/PD

![BSS coloring comparison](figures/bss/bss-coloring-comparison.png)

IEEE Std 802.11-2024 Clause 26.10 distinguishes OBSS/PD-based and parameterized spatial reuse (`80211ax-2024:chunk:09886`). The Spatial Reuse Parameter Set carries OBSS/PD bounds and BSS-color information (Clause 9.4.2.251, `80211ax-2024:chunk:03659`–`03661`). A more permissive threshold can enable concurrency, but it can also increase interference; same-color BSSs must not be treated as distinct OBSSs for this decision.

Both BSSs use the common `0.2–0.25 s` warm-up and start normal traffic at
`0.3 s`. BSS 1 remains at `(200,250)`. BSS 2 starts at `(260,250)` and moves
along the positive x axis at `200 m/s`; its STAs move with it so every wanted
AP-to-STA distance stays at 30 m. During the `0.3–0.95 s` measurement window,
the AP separation therefore increases from 120 m to 250 m. Normal downlink
traffic uses independent intervals uniformly distributed between `1 ms` and
`1.4 ms`. This keeps both APs backlogged without synchronizing all four
application streams.
The medium limit cache is raised to `100 ms` because this load can create
aggregated transmissions longer than its generic `10 ms` default. Every AP and
STA is assigned its BSS color; a zero local color would disable OBSS/PD
classification in the receiver.

The moving geometry makes the OBSS signal cross `-78 dBm`, `-79 dBm`, and
`-81 dBm` at different times. The decisive third panel measures simultaneous
AP transmission using `transmissionState == 2`, the actual `TRANSMITTING` enum
value. In the refreshed five-seed results, disabled produces
`6.831 ± 0.276 Mbps` and `0.444 ± 0.178%` concurrent AP airtime. Conservative
(`-81 dBm`), enabled (`-79 dBm`), and aggressive (`-78 dBm`) increase those
values progressively to `7.151 ± 0.165`, `7.303 ± 0.399`, and
`7.606 ± 0.345 Mbps`, with concurrent airtime of `2.206 ± 1.659%`,
`5.334 ± 0.761%`, and `8.248 ± 3.426%`, respectively (95% Student-t
confidence intervals). Mean Jain fairness stays between 0.939 and 0.992. The
same-color control exactly reproduces disabled, proving that correct BSS
classification is necessary for the gain.
