---
name: omnetpp-result-plotting
description: Create reproducible, non-interactive plots and derived summaries from OMNeT++ .sca and .vec results with the native Python result-analysis API. Use to visualize scalar, vector, statistic, or histogram results; compare configurations or repetitions; compute confidence intervals, ECDFs, or time-weighted summaries; and generate plotting scripts and figure artifacts.
---

# Plot OMNeT++ results

Use `from omnetpp.scave import results`; do not parse result files manually or use CSV as an intermediate data source. Run Python in the configured OMNeT++ environment.

## Workflow

1. Select the exact `.sca` and `.vec` inputs for the requested configuration and runs. Do not load an entire mixed result tree by default.
2. Discover actual result names, modules, units, runs, and iteration variables. When these are not already established, run:

   ```sh
   python .agents/skills/omnetpp-result-plotting/scripts/inspect_results.py \
     results/run.sca results/run.vec
   ```

   Add `--filter '<OMNeT++ result filter>'` to narrow a large result set.
3. Define the measurement before writing plotting code:
   - result type and filter expression;
   - experimental-condition columns and independent repetition identifier;
   - included modules and their aggregation;
   - time window or warm-up removal;
   - units and conversion;
   - per-run reduction and plot type.
4. Query with the native API. Preserve metadata through the final aggregation:

   ```python
   query_options = dict(
       include_attrs=True,
       include_runattrs=True,
       include_itervars=True,
   )
   results.set_inputs(input_files)
   frame = results.get_scalars(filter_expression, **query_options)
   ```

   Add `include_config_entries=True` only when configuration entries define or disambiguate conditions; it can add many columns. Use `get_vectors`, `get_statistics`, or `get_histograms` when appropriate. For large vectors, pass `start_time` and `end_time` to `get_vectors`.
5. Validate before transforming:
   - reject empty queries;
   - report matched `runID`, `module`, `name`, and `unit`;
   - reject missing required columns, incompatible units, and unexpected duplicates;
   - for vectors, verify nonempty equal-length `vectime`/`vecvalue` arrays and monotonic timestamps;
   - verify all intended conditions and repetition counts.
6. Reduce to the correct observational unit, then plot. Keep extraction, transformation, and rendering as separate stages in the saved analysis script.
7. Save the script and figure under the analysis output directory. Report inputs, filter, runs, modules, window, aggregation, uncertainty method, unit conversion, missing data, and display-only downsampling.

## Statistical and semantic rules

- Treat independent runs—not packets, nodes, or vector samples—as repetitions. Compute uncertainty from one justified estimate per run.
- Include every varying experimental parameter in the condition key. Do not group only by the x-axis variable.
- Do not pool vector samples across runs without stating the weighting effect; longer vectors otherwise receive more weight.
- Use a time-weighted mean for piecewise-constant event signals such as queue length or state. A sample mean is generally wrong for them.
- Do not invent a warm-up interval. Use experiment configuration or user direction as evidence.
- Preserve recorded units and convert them explicitly. Never infer a unit from the result name when metadata is available.
- Aggregate multiple modules only when the metric defines a meaningful operation such as sum, mean, or weighted mean.

Read [analysis-patterns.md](references/analysis-patterns.md) when implementing parameter-study confidence intervals, per-run vector reduction, time-weighted means, ECDFs, cumulative-counter rates, or large-vector handling.

## Plot selection

| Recorded quantity | Default representation |
|---|---|
| Continuous or regularly sampled value | line |
| Piecewise-constant state or counter | step (`where="post"`) |
| Independent per-packet observations over time | scatter |
| Metric versus numeric parameter | line/markers with per-run uncertainty |
| Metric versus categorical condition | points, bars, or box plot |
| Sample distribution | ECDF; histogram when binning is useful |

Do not connect unrelated observations or categorical values. Avoid raw plots with unreadable numbers of runs, modules, or points; summarize, facet, select, or downsample for display while retaining full data for computation.

For a direct vector plot with no custom reduction, use:

```sh
python .agents/skills/omnetpp-result-plotting/scripts/plot_vector.py \
  results/run.vec \
  --filter 'module =~ "Network.host[*].queue" AND name =~ "queueLength:vector"' \
  --kind step \
  --ylabel 'Queue length [packets]' \
  --output analysis/figures/queue-length.png
```

For parameter studies or derived metrics, create a task-specific script using the reference patterns instead of forcing the generic vector plotter to encode experiment semantics.

## Completion check

Confirm that the saved figure:

- is generated non-interactively and deterministically;
- labels axes and units and identifies multiple series;
- uses a plot type consistent with the recorded quantity;
- is based on an explicit, reported query and validated run set;
- documents aggregation, uncertainty, time-window, and downsampling choices.
