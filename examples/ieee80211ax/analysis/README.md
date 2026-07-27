# IEEE 802.11ax result analysis

The authoritative user interface is now the generation-neutral
[`wifi_analysis.py`](../../ieee80211/analysis/wifi_analysis.py) facade. For
example:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect bss_coloring
python3 examples/ieee80211/analysis/wifi_analysis.py run bss_coloring \
  --evidence both --runs 5
python3 examples/ieee80211/analysis/wifi_analysis.py report bss_coloring \
  --session-id <YYYYMMDDTHHMMSSZ>
python3 examples/ieee80211/analysis/wifi_analysis.py publish bss_coloring \
  --session-id <YYYYMMDDTHHMMSSZ> --update
```

The scalar/vector drivers currently live in this directory and serve the AX
suite plus the mapped `eht_features` scenario.
The generation-neutral PCAP implementation lives under
`examples/ieee80211/analysis`. Only `publish ... --update` is intended to edit
walkthroughs. A one-run session is diagnostic; publication requires at least
five independent runs. Measurement windows use `[start, end)` notation.

These figures are evidence checks for the INET HE implementation, not generic claims that a feature always improves throughput. Every bar is computed from five independent simulation runs; error bars are two-sided 95% Student-t confidence intervals over run-level observations. Timelines and ECDFs use run 0 only and are labeled accordingly, so packet samples are never treated as independent repetitions.

The experiment manifest is [`experiments.json`](experiments.json). It fixes the configuration names, repetition count, measurement windows, workload metadata, output paths, and uniquely identified evidence contracts. Result directories are derived from each simulation example's INI path. Each contract names an evaluator. `mimo_disjoint_streams` and `matched_delivery_ratio` are authoritative; declarations whose decisive evidence is not yet executable explicitly use `unimplemented` and evaluate `INCONCLUSIVE`. The loader rejects missing `.sca`/`.vec` pairs, extra or mixed configurations, missing runs, non-monotonic vectors, misaligned telemetry, and unit mismatches where OMNeT++ records a unit. `packetBytes` recorder values are bytes even though OMNeT++ currently leaves their unit attribute empty; the exact recorder name is used as that contract.

The controlled short experiments use the same phase convention: one low-rate
warm-up trigger in `0.2–0.25 s`, normal traffic from `0.3 s`, and analysis
windows beginning at `0.3 s` unless a feature needs a settling interval (rate
adaptation) or a different time scale (TWT). The downlink example also uses
`warmup-period = 0.25 s` so simulator result statistics do not discard part of
the common warm-up definition.

## Conclusions

| Analysis | Evidence required before accepting the plot | Interpretation |
|---|---|---|
| [Dynamic fragmentation](../dynamic_frag/walkthrough.md) | MAC-frame bytes and measured ACK airtime | Fragmentation changes the transmitted-size distribution and acknowledgment work; dynamic and static policies can be identical when given the same threshold. |
| [UORA](../ul_uora/walkthrough.md) | Nonzero per-STA attempts and successes for every condition | MixedUora is reported as a separate three-STA reference; under the matched heavy load, five RA-RUs record about 9× the successful UORA transmissions and higher success fairness than one RA-RU. |
| [TWT](../twt/walkthrough.md) | Integrated radio power and TWT delivery at least 95% of baseline | Energy savings are accepted only when they are not obtained by dropping the workload. |
| [Rate adaptation](../rate_adaptation/walkthrough.md) | Selected MCS/NSS, EWMA probability, and actual TX outcome | A changing MCS is adaptation evidence only when paired with transmission outcomes. |
| [Preamble puncturing](../preamble_puncturing/walkthrough.md) | Runtime mask 0 → 2 → 0 and aligned RU placement | Puncturing avoids a busy secondary 20 MHz channel but sacrifices usable spectrum. |
| [MU-MIMO](../dl_mu_mimo/walkthrough.md) | Multiple users in a PPDU and disjoint spatial-stream ranges | Concurrent streams, not throughput alone, establish that MU-MIMO occurred. |
| [BSS coloring](../bss_coloring/walkthrough.md) | Correct color classification, OBSS/PD decisions, and concurrent AP airtime | A moving OBSS makes nearby thresholds yield more spatial reuse in the retained scenario, while the same-color control reproduces disabled. |
| [Channel width](../channel_widths/walkthrough.md) | Saturated workload and per-run goodput/delay | Wider channels increase capacity here, but scaling is not expected to be perfectly linear. |
| [DL schedulers](../dl_ofdma_sched/walkthrough.md) and [asymmetric DL](../dl_ofdma_asym/walkthrough.md) | Separate symmetric/asymmetric workloads | Scheduler conclusions depend on load shape; asymmetric fairness is normalized by offered load. |
| [BSR](../bsr/walkthrough.md) | AP-reported and AP-scheduled backlog timelines | BSR is scheduling state, not application goodput; freshness controls whether the AP view is usable. |
| [Dense IoT](../dense_iot/README.md) | Matched AX/AC campaigns across station counts, workloads, delivery, delay, and energy | This broader campaign has its own runner and result analyzer because each configuration combines station-count iterations with repetitions. |

Machine-readable plotted summaries are in [`metrics.json`](metrics.json).
Each timestamped result directory contains both the scalar/vector figure and
the PCAP figure, plus their `.png.json` provenance sidecars. The directory
itself identifies the exact raw result session, so figure sidecars retain
filters, measurement windows, aggregation rules, and tool/source metadata
without repeating every raw input path and hash.
`render_walkthrough_results.py` turns those session-bound metrics and figures
into a compact marker-bounded table and plot link inside each walkthrough's
scalar/vector section; it does not replace the authored query, aggregation,
and interpretation text. Generated headings are labeled `[script]`. With
`--update`, the renderer also refreshes the scalar/vector entry in the
script-owned results-session ledger below the walkthrough title while
preserving the PCAP and agent-owned entries. The feature-specific analysis
narratives are consolidated into those walkthroughs so the configuration,
evidence, verdict, and debugging guidance remain together.

## Reproduce and verify

The generation-neutral campaign, suite, and typed PHY-profile primitives live
under [`examples/ieee80211/analysis`](../../ieee80211/analysis/README.md).
These AX scalar/vector commands use the shared campaign primitives. The shared
PCAP implementation runs the complete AX suite descriptor or an EHT suite
without changing the analyzer.

For AX scalar/vector development and testing, from the repository root:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py all
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211ax/analysis/first_tranche.py all
python3 examples/ieee80211ax/analysis/summarize_results.py
python3 examples/ieee80211ax/analysis/render_walkthrough_results.py all --update
python3 examples/ieee80211ax/analysis/evaluate_evidence.py
MPLCONFIGDIR=/tmp/matplotlib python3 -m unittest discover \
    -s examples/ieee80211ax/analysis -p 'test_*.py'
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211ax/analysis/first_tranche.py all --check
```

Index packet evidence recorded by a shared campaign, or reanalyze the
manifest-validated captures, with:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --index --capture-only --session-id 20260725T120000Z \
  --subdir dl_ofdma_sched --run 0
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --reuse --session-id 20260725T120000Z --subdir dl_ofdma_sched --run 0
```

The analyzer publishes immutable capture-index history at
`examples/ieee80211/analysis/generated/ax/capture_manifests/<session-id>.json`
before atomically replacing
`examples/ieee80211/analysis/generated/ax/capture_manifest.json`
with the same full latest-session mirror.
`--reuse --session-id ...` selects exact history; omitting the ID selects the
latest mirror. The old standalone packet-statistics manifests and generator
are removed. Each capture-manifest entry
records the scalar hash and run attributes plus `capinfos` file, link,
size/count, timestamp-order, precision, snapshot, and interface metadata.

The PCAP analyzer validates every capture before parsing and writes a bounded
frame-exchange timeline, compact cross-configuration table, and
count-versus-airtime plot. The plot and provenance sidecar are stored at
`results/<session-id>/packet_statistics.png[.json]`, alongside the raw
configuration result directories. Generated walkthrough sections reference
that session-local copy. Reuse mode
does not edit walkthroughs by default; `--update-walkthrough` is explicit and
is reserved for the facade's publish step. When enabled, it labels generated
headings `[script]` and refreshes the PCAP entry in the script-owned
results-session ledger without changing the scalar/vector or agent-owned
entries. It exits nonzero when an evidence
check is `FAIL`. `--allow-failed-evidence` is intended only for preserving an
exploratory report whose failed checks will be investigated.

`evaluate_evidence.py` atomically writes `evidence-ledger.json`. Its normal
exit status is 1 only when an executable check is `FAIL`; add
`--require-conclusive` to require every selected check to be `PASS`. An
explicit result session missing from one group becomes check-level `NOT RUN`
for this ledger rather than aborting the other groups:

```sh
python3 examples/ieee80211ax/analysis/evaluate_evidence.py \
    --session-id 20260725T120000Z --group mimo --group twt
python3 examples/ieee80211ax/analysis/evaluate_evidence.py --require-conclusive
```

Source hashing covers staged and unstaged tracked changes relative to `HEAD`
plus relevant non-ignored, untracked inputs. Generated walkthroughs, figures,
report outputs, and immutable manifest-history files are excluded so
publishing evidence does not stale itself.

The shared facade maps Dense IoT to a five-run, eight-station comparison so
its run-0 PCAP comes from the same result set. The broader 8/16-station
campaign retains its iteration-aware runner and analyzer:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211ax/dense_iot/analyze.py
```

The campaign runner uses Cmdenv and executes independent configuration/run pairs in parallel. Each invocation stores one result set as `results/YYYYMMDDTHHMMSSZ/<configuration>/`; pass `--session-id` to reuse an explicit UTC session identifier. A combined campaign enables PCAP recording only for run 0, so scalar/vector analysis uses runs 0–4 while PCAP analysis uses the captures from run 0 of that same set. The analysis commands select the newest complete session by default, or accept the same `--session-id` explicitly. `summarize_results.py` rebuilds `metrics.json` atomically instead of retaining stale groups, records the selected session and input hashes per group, and requires an explicitly requested session to be complete for the entire manifest. By default the runner uses all CPUs available to the process, equivalent to `$(nproc)`; pass `-j N` to tune concurrency for the machine or `-j 1` for serial execution. Each run sets `seed-set` to its repetition number and records only the vectors/scalars needed by these analyses. Successful Cmdenv output is suppressed to keep parallel logs readable, while output from failed runs is replayed in full. Raw result sets and their generated figures remain ignored together.

The normative reference is IEEE Std 802.11-2024 in the repository corpus (`80211ax-2024`). Relevant clause/chunk identifiers are listed on each analysis page. These experiments validate selected observable consequences of those procedures; they are not a conformance certification.
