# IEEE 802.11ax result analysis

These figures are evidence checks for the INET HE implementation, not generic claims that a feature always improves throughput. Every bar is computed from five independent simulation runs; error bars are two-sided 95% Student-t confidence intervals over run-level observations. Timelines and ECDFs use run 0 only and are labeled accordingly, so packet samples are never treated as independent repetitions.

The experiment manifest is [`experiments.json`](experiments.json). It fixes the configuration names, result directories, repetition count, measurement windows, workload metadata, and output paths. The loader rejects missing `.sca`/`.vec` pairs, extra or mixed configurations, missing runs, non-monotonic vectors, misaligned telemetry, and unit mismatches where OMNeT++ records a unit. `packetBytes` recorder values are bytes even though OMNeT++ currently leaves their unit attribute empty; the exact recorder name is used as that contract.

The controlled short experiments use the same phase convention: one low-rate
warm-up trigger in `0.2–0.25 s`, normal traffic from `0.3 s`, and analysis
windows beginning at `0.3 s` unless a feature needs a settling interval (rate
adaptation) or a different time scale (TWT). The downlink example also uses
`warmup-period = 0.25 s` so simulator result statistics do not discard part of
the common warm-up definition.

## Conclusions

| Analysis | Evidence required before accepting the plot | Interpretation |
|---|---|---|
| [Dynamic fragmentation](fragmentation.md) | MAC-frame bytes and measured ACK airtime | Fragmentation changes the transmitted-size distribution and acknowledgment work; dynamic and static policies can be identical when given the same threshold. |
| [UORA](uora.md) | Nonzero attempts and successes for every condition | More RA-RUs should reduce contention pressure, but random outcomes require confidence intervals. |
| [TWT](twt.md) | Integrated radio power and TWT delivery at least 95% of baseline | Energy savings are accepted only when they are not obtained by dropping the workload. |
| [Rate adaptation](rate-adaptation.md) | Selected MCS/NSS, EWMA probability, and actual TX outcome | A changing MCS is adaptation evidence only when paired with transmission outcomes. |
| [Preamble puncturing](puncturing.md) | Runtime mask 0 → 2 → 0 and aligned RU placement | Puncturing avoids a busy secondary 20 MHz channel but sacrifices usable spectrum. |
| [MU-MIMO](mu-mimo.md) | Multiple users in a PPDU and disjoint spatial-stream ranges | Concurrent streams, not throughput alone, establish that MU-MIMO occurred. |
| [BSS coloring](bss-coloring.md) | Correct color classification, OBSS/PD decisions, and concurrent AP airtime | A moving OBSS makes nearby thresholds yield a strict disabled < conservative < enabled < aggressive goodput ladder (6.83–7.61 Mbit/s), while the same-color control reproduces disabled. |
| [Channel width](channel-width.md) | Saturated workload and per-run goodput/delay | Wider channels increase capacity here, but scaling is not expected to be perfectly linear. |
| [DL schedulers](dl-schedulers.md) | Separate symmetric/asymmetric workloads | Scheduler conclusions depend on load shape; asymmetric fairness is normalized by offered load. |
| [BSR](bsr.md) | AP-reported and AP-scheduled backlog timelines | BSR is scheduling state, not application goodput; freshness controls whether the AP view is usable. |

Machine-readable plotted summaries are in [`metrics.json`](metrics.json). Each PNG has a `.png.json` provenance sidecar containing the exact inputs, SHA-256 hashes, filters, measurement window, aggregation rule, and source revision.

## Reproduce and verify

From the repository root:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py all
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211ax/analysis/first_tranche.py all
python3 examples/ieee80211ax/analysis/summarize_results.py
python3 examples/ieee80211ax/analysis/test_analysis_core.py
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211ax/analysis/first_tranche.py all --check
```

The campaign runner uses Cmdenv and executes independent configuration/run pairs in parallel. By default it uses all CPUs available to the process, equivalent to `$(nproc)`; pass `-j N` to tune concurrency for the machine or `-j 1` for serial execution. Each run sets `seed-set` to its repetition number and records only the vectors/scalars needed by these analyses. Successful Cmdenv output is suppressed to keep parallel logs readable, while output from failed runs is replayed in full. Raw `.sca`, `.vec`, and `.vci` files stay ignored because they are large; the checked-in artifacts are the figures, provenance sidecars, and `metrics.json`. CI regenerates the five-seed campaign before running the stale-figure and metric checks.

The normative reference is IEEE Std 802.11-2024 in the repository corpus (`80211ax-2024`). Relevant clause/chunk identifiers are listed on each analysis page. These experiments validate selected observable consequences of those procedures; they are not a conformance certification.
