# Uplink OFDMA random access (UORA)

![UORA dashboard](figures/uora/uora-dashboard.png)

IEEE Std 802.11-2024 Clauses 26.5.4.1–26.5.4.3 define the OFDMA contention window, OBO countdown, eligible RA-RUs, and UORA transmission procedure (`80211ax-2024:chunk:09810`–`09812`). Consequently, success is probabilistic and depends on both offered contention and the number of RA-RUs.

Eight STAs are measured under light load with one RA-RU, heavy load with one
RA-RU, and heavy load with five RA-RUs. Light and heavy use 100-byte packets at
`4 ms` and `1 ms` intervals, respectively. Five RUs were selected from a
1/3/5/7/9-RU run-0 sweep: it exposes the access-capacity gain while retaining
scheduled capacity in the mixed scheduler. The plot uses explicit per-STA
attempt and success counters. Generation fails if any condition has no attempts
or if all successes are zero.

The refreshed results show `27.6 ± 18.4` successful transmissions/run and
`0.675 ± 0.051` success probability for light load, `45.8 ± 27.3` and
`0.599 ± 0.068` for heavy load with one RA-RU, and `214.2 ± 128.6` and
`0.684 ± 0.025` with five RA-RUs (95% Student-t intervals). Jain fairness of
per-STA successes rises from `0.651 ± 0.168` in the heavy one-RU condition to
`0.890 ± 0.049` with five RUs. Thus the tuned allocation produces roughly
4.7 times as many successful UORA transmissions and distributes them more
evenly; it does not imply that reserving five RUs is optimal for every mix of
scheduled and random-access traffic.
