# Uplink OFDMA random access (UORA)

![UORA dashboard](figures/uora/uora-dashboard.png)

IEEE Std 802.11-2024 Clauses 26.5.4.1–26.5.4.3 define the OFDMA contention window, OBO countdown, eligible RA-RUs, and UORA transmission procedure (`80211ax-2024:chunk:09810`–`09812`). Consequently, success is probabilistic and depends on both offered contention and the number of RA-RUs.

Eight STAs are measured under light load with one RA-RU, heavy load with one RA-RU, and heavy load with three RA-RUs. The plot uses explicit per-STA attempt and success counters. Generation fails if any condition has no attempts or if all successes are zero, preventing the previous all-zero chart from being accepted as evidence.

The expected pattern is lower success probability and more unsuccessful attempts when load rises, followed by relief when three RA-RUs are available. Jain fairness is calculated from per-STA successes within each run. Small departures from monotonicity are plausible with five seeds; use the confidence intervals rather than a single-run ordering.
