---
name: omnetpp-result-analysis
description: Inspect, filter, query, and export OMNeT++ scalar and vector result files using opp_scavetool. Use after a simulation has generated .sca or .vec files, or when asked to find, compare, or extract recorded simulation statistics.
---

# Analyzing OMNeT++ results

Select `.sca` and `.vec` files using run metadata; do not assume every file in a result directory belongs to the requested configuration or run.

## Workflow

1. Identify the configuration and run.
2. Query the relevant files and list available module/result names.
3. Construct the narrowest result-selection expression.
4. Verify result type, units, module path, name, and run attributes.
5. Report every ambiguous match instead of silently selecting one.
6. Export only the items and vector interval needed for further analysis.

## Query and export

```sh
opp_scavetool query results/run.sca results/run.vec

opp_scavetool query \
  -l \
  -f '<filter>' \
  results/run.sca \
  results/run.vec

opp_scavetool export \
  -f '<filter>' \
  -F CSV-R \
  -o results.csv \
  results/run.sca \
  results/run.vec
```

Quote result-selection expressions in the shell. Do not overwrite an existing analysis export unless requested. Allow `opp_scavetool` to manage vector indexes unless a concrete indexing problem requires intervention.

## Interpretation

Distinguish scalars, vectors, statistics, and histograms. For vectors, correlate timestamps with captures, logs, or event logs when investigating a specific transition; aggregates alone may hide the causal event.

Before comparing runs, confirm compatible configurations, iteration variables, units, warm-up periods, recording settings, binaries, and seeds where relevant.

Report input files, filter expression, run identifiers, matching module/result names, types, values and units, exported files, and empty or ambiguous matches.
