---
name: omnetpp-result-plotting
description: Plot OMNeT++ scalar and vector results using the native Python result-analysis API. Use after a simulation has generated .sca or .vec files, or when asked to visualize recorded simulation statistics.
---
# OMNeT++/INET Simulation Result Plotting

## Purpose

Use this skill when analyzing and plotting results produced by OMNeT++ or INET simulations from `.sca` and `.vec` files.

Use only the native OMNeT++ Python result-analysis API:

```python
from omnetpp.scave import results
```

Do not:

* Parse `.sca` or `.vec` files manually.
* Export results through `opp_scavetool`.
* Convert results to CSV as an intermediate step.
* Assume result names, module paths, units, iteration variables, or run structure without inspecting the data.
* Use the OMNeT++ graphical IDE as part of the workflow.

The expected environment is OMNeT++ 6.1 or later.

---

## Required working principles

### 1. Inspect before plotting

Never write a final plotting query based only on a presumed INET result name.

First inspect the available:

* Runs
* Scalars
* Vectors
* Statistics
* Histograms
* Module paths
* Result names
* Units
* Run attributes
* Iteration variables

INET module paths and result names depend on the network structure, applications, protocol configuration and result-recording configuration.

### 2. Preserve experiment metadata

When reading results for a parameter study, request:

```python
include_attrs=True
include_runattrs=True
include_itervars=True
include_config_entries=True
```

Retain relevant metadata until the final aggregation has been completed.

Useful metadata commonly includes:

* `runID`
* `experiment`
* `measurement`
* `replication`
* `repetition`
* `seedset`
* Configuration name
* Network name
* Iteration variables
* Module path
* Result name
* Unit

Do not aggregate runs before identifying which columns distinguish experimental conditions and repetitions.

### 3. Distinguish scalar and vector semantics

A scalar is one recorded value associated with a run and module.

Typical scalar analyses include:

* Metric versus parameter value
* Mean metric by configuration
* Confidence intervals over repetitions
* Per-node comparisons
* Bar plots and box plots

A vector contains samples indexed by simulation time.

Typical vector analyses include:

* Time-series plots
* State transitions
* Queue evolution
* Packet delay samples
* Throughput over time
* Histograms
* Empirical cumulative distribution functions
* Time-window summaries

Do not treat vector samples as statistically independent repetitions. Samples inside one vector normally belong to the same simulation run.

### 4. Separate extraction, transformation and plotting

Structure analysis code into three stages:

1. Load and query OMNeT++ results.
2. Validate, reshape and aggregate data.
3. Generate and save the plot.

Avoid combining all operations into a single opaque expression.

### 5. Validate every query

After every `results.get_*()` call:

* Check whether the returned DataFrame is empty.
* Print or report the matched module paths and result names.
* Verify expected units.
* Verify the number of runs.
* Verify the number of repetitions per condition.
* Check for missing values.
* Check whether vector time and value arrays have matching lengths.

Fail with a clear error instead of silently producing an empty or misleading plot.

---

## Environment setup

The Python script must run in an OMNeT++-configured shell.

Typical setup:

```bash
cd /path/to/omnetpp
source setenv
```

Then verify the API:

```bash
python -c "from omnetpp.scave import results; print('omnetpp.scave available')"
```

The analysis normally requires:

```bash
python -m pip install numpy pandas matplotlib scipy
```

Prefer a non-interactive Matplotlib backend for automated agents, servers and CI environments:

```python
import matplotlib

matplotlib.use("Agg")
```

Set the backend before importing `matplotlib.pyplot`.

---

## Standard project layout

Prefer the following structure:

```text
project/
├── omnetpp.ini
├── simulations/
├── results/
│   ├── General-0.sca
│   ├── General-0.vec
│   ├── General-1.sca
│   └── General-1.vec
└── analysis/
    ├── inspect_results.py
    ├── plot_vectors.py
    ├── plot_scalars.py
    └── figures/
```

Write generated figures under a dedicated directory rather than beside the raw result files.

---

## Loading result files

### Stateful API

Use this form for small standalone scripts:

```python
from omnetpp.scave import results

results.set_inputs("results/")
```

You can then call:

```python
scalars = results.get_scalars("*")
vectors = results.get_vectors("*")
```

### Stateless API

Prefer explicit loading in reusable modules, larger programs and tests:

```python
from omnetpp.scave import results

raw_results = results.read_result_files(
    "results/",
    filter_expression="*",
)

scalars = results.get_scalars(raw_results)
vectors = results.get_vectors(raw_results)
```

The stateless form makes dependencies explicit and avoids hidden global input state.

---

## Mandatory discovery workflow

Before implementing a plot, create or run an inspection script.

```python
#!/usr/bin/env python3

from pathlib import Path

import pandas as pd
from omnetpp.scave import results


RESULTS_DIR = Path("results")


def show_unique_results(df: pd.DataFrame, result_type: str) -> None:
    print(f"\n{result_type}")
    print("=" * len(result_type))

    if df.empty:
        print("No results found.")
        return

    columns = [
        column
        for column in ("module", "name", "unit")
        if column in df.columns
    ]

    print(
        df[columns]
        .drop_duplicates()
        .sort_values(columns)
        .to_string(index=False)
    )


def main() -> None:
    results.set_inputs(str(RESULTS_DIR))

    common_options = {
        "include_attrs": True,
        "include_runattrs": True,
        "include_itervars": True,
        "include_config_entries": True,
    }

    scalars = results.get_scalars("*", **common_options)
    vectors = results.get_vectors("*", **common_options)
    statistics = results.get_statistics("*", **common_options)
    histograms = results.get_histograms("*", **common_options)

    show_unique_results(scalars, "Scalars")
    show_unique_results(vectors, "Vectors")
    show_unique_results(statistics, "Statistics")
    show_unique_results(histograms, "Histograms")

    run_ids = set()

    for frame in (scalars, vectors, statistics, histograms):
        if "runID" in frame.columns:
            run_ids.update(frame["runID"].dropna().astype(str))

    print(f"\nRuns found: {len(run_ids)}")

    for run_id in sorted(run_ids):
        print(run_id)


if __name__ == "__main__":
    main()
```

Use the inspection output to construct the actual filter expression.

---

## OMNeT++ filter expressions

Use OMNeT++ result filters rather than filtering everything in Pandas after loading.

Typical fields include:

```text
type
run
module
name
attr:<attribute-name>
runattr:<run-attribute-name>
itervar:<iteration-variable-name>
config:<configuration-entry>
```

Examples:

```python
'name =~ "endToEndDelay:vector"'
```

```python
'module =~ "Network.host[*].app[*]" AND name =~ "packetSent:count"'
```

```python
'type =~ vector AND name =~ "*Throughput*"'
```

```python
'itervar:numHosts =~ "20" AND name =~ "packetReceived:count"'
```

Filter syntax is not Python syntax.

Do not use Python operators such as:

```python
module == "..."
```

inside an OMNeT++ result filter.

### Matching module paths

Be deliberate about wildcards.

A module path may look like:

```text
Network.host[3].wlan[0].mac.queue
```

Use a narrow module filter whenever possible.

Examples:

```python
'module =~ "Network.host[*].app[0]"'
```

```python
'module =~ "Network.host[*].wlan[0].mac.**"'
```

After applying a filter, always inspect the matched `(module, name)` pairs.

---

## Querying scalars

Use:

```python
scalars = results.get_scalars(
    filter_expression,
    include_attrs=True,
    include_runattrs=True,
    include_itervars=True,
    include_config_entries=True,
)
```

Important columns typically include:

```text
runID
module
name
value
```

Additional metadata columns depend on the recording configuration and query options.

### Scalar validation helper

```python
import pandas as pd


def validate_scalars(
    frame: pd.DataFrame,
    expected_unit: str | None = None,
) -> None:
    if frame.empty:
        raise RuntimeError("The scalar query returned no results.")

    required_columns = {"runID", "module", "name", "value"}
    missing = required_columns.difference(frame.columns)

    if missing:
        raise RuntimeError(
            f"Scalar data is missing required columns: {sorted(missing)}"
        )

    frame["value"] = pd.to_numeric(frame["value"], errors="raise")

    if expected_unit is not None:
        if "unit" not in frame.columns:
            raise RuntimeError(
                f"Expected unit {expected_unit!r}, but no unit column exists."
            )

        units = set(frame["unit"].dropna().astype(str))

        if units != {expected_unit}:
            raise RuntimeError(
                f"Expected unit {expected_unit!r}, found {sorted(units)}."
            )
```

---

## Querying vectors

Use:

```python
vectors = results.get_vectors(
    filter_expression,
    include_attrs=True,
    include_runattrs=True,
    include_itervars=True,
    include_config_entries=True,
)
```

A vector row normally contains:

```text
runID
module
name
vectime
vecvalue
```

`vectime` and `vecvalue` contain array-like sample data.

### Vector validation helper

```python
import numpy as np
import pandas as pd


def validate_vectors(
    frame: pd.DataFrame,
    expected_unit: str | None = None,
) -> None:
    if frame.empty:
        raise RuntimeError("The vector query returned no results.")

    required_columns = {
        "runID",
        "module",
        "name",
        "vectime",
        "vecvalue",
    }
    missing = required_columns.difference(frame.columns)

    if missing:
        raise RuntimeError(
            f"Vector data is missing required columns: {sorted(missing)}"
        )

    for index, row in frame.iterrows():
        times = np.asarray(row["vectime"], dtype=float)
        values = np.asarray(row["vecvalue"], dtype=float)

        if len(times) != len(values):
            raise RuntimeError(
                f"Vector row {index} has {len(times)} timestamps "
                f"but {len(values)} values."
            )

        if len(times) == 0:
            raise RuntimeError(f"Vector row {index} contains no samples.")

        if np.any(np.diff(times) < 0):
            raise RuntimeError(
                f"Vector row {index} has non-monotonic timestamps."
            )

    if expected_unit is not None:
        if "unit" not in frame.columns:
            raise RuntimeError(
                f"Expected unit {expected_unit!r}, but no unit column exists."
            )

        units = set(frame["unit"].dropna().astype(str))

        if units != {expected_unit}:
            raise RuntimeError(
                f"Expected unit {expected_unit!r}, found {sorted(units)}."
            )
```

---

## Plotting a vector time series

Use a line plot only when interpolation between samples has a meaningful interpretation.

```python
#!/usr/bin/env python3

from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
from omnetpp.scave import results


RESULTS_DIR = Path("results")
OUTPUT_FILE = Path("analysis/figures/end-to-end-delay.png")

FILTER = (
    'module =~ "Network.host[*].app[*]" '
    'AND name =~ "endToEndDelay:vector"'
)


def main() -> None:
    results.set_inputs(str(RESULTS_DIR))

    vectors = results.get_vectors(
        FILTER,
        include_attrs=True,
        include_runattrs=True,
        include_itervars=True,
        include_config_entries=True,
    )

    if vectors.empty:
        raise RuntimeError(f"No vectors matched filter: {FILTER}")

    matched = (
        vectors[["module", "name"]]
        .drop_duplicates()
        .sort_values(["module", "name"])
    )
    print("Matched vectors:")
    print(matched.to_string(index=False))

    figure, axis = plt.subplots(figsize=(10, 5))

    for _, row in vectors.iterrows():
        times = np.asarray(row["vectime"], dtype=float)
        values = np.asarray(row["vecvalue"], dtype=float)

        if len(times) != len(values):
            raise RuntimeError(
                f"Mismatched vector arrays for {row['module']}."
            )

        label = str(row["module"])

        axis.plot(
            times,
            values,
            label=label,
            linewidth=1.2,
        )

    axis.set_xlabel("Simulation time [s]")
    axis.set_ylabel("End-to-end delay [s]")
    axis.set_title("End-to-end delay over simulation time")
    axis.grid(True, alpha=0.3)
    axis.legend(fontsize="small")

    figure.tight_layout()

    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(OUTPUT_FILE, dpi=200, bbox_inches="tight")
    plt.close(figure)

    print(f"Created {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
```

---

## Choosing the correct vector plot

### Continuous or sampled quantity

Use a line plot for quantities such as:

* Estimated throughput
* Channel utilization
* Smoothed signal quality
* Continuously sampled physical values

```python
axis.plot(times, values)
```

### Piecewise-constant state

Use a step plot for:

* Queue length
* Radio state
* MAC state
* Number of active connections
* Gate or protocol state
* Counters whose value changes at events

```python
axis.step(
    times,
    values,
    where="post",
)
```

### Per-packet observations

Use a scatter plot for:

* End-to-end delay per packet
* Packet size observations
* Per-packet signal-to-noise ratio
* Sequence-number-related measurements

```python
axis.scatter(
    times,
    values,
    s=8,
)
```

Do not connect unrelated packet observations merely because they occur in time order.

---

## Restricting vectors to a time interval

For large vectors, restrict the time range during result loading when the API supports it:

```python
vectors = results.get_vectors(
    FILTER,
    include_attrs=True,
    include_runattrs=True,
    include_itervars=True,
    start_time=10.0,
    end_time=100.0,
)
```

Alternatively, crop explicitly:

```python
mask = (times >= start_time) & (times <= end_time)
cropped_times = times[mask]
cropped_values = values[mask]
```

Prefer query-time cropping when processing large result sets.

State clearly in labels, captions or filenames when a warm-up interval has been removed.

Example:

```text
mean-delay-after-warmup.png
```

---

## Plotting scalar results from a parameter study

A parameter-study plot normally aggregates repetitions by experimental condition.

```python
#!/usr/bin/env python3

from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy.stats import t
from omnetpp.scave import results


RESULTS_DIR = Path("results")
OUTPUT_FILE = Path("analysis/figures/throughput-vs-num-hosts.png")

FILTER = 'name =~ "throughput:mean"'
PARAMETER_COLUMN = "numHosts"


def summarize_with_ci95(
    frame: pd.DataFrame,
    group_column: str,
) -> pd.DataFrame:
    summary = (
        frame.groupby(group_column, as_index=False)["value"]
        .agg(
            mean="mean",
            standard_deviation="std",
            count="count",
        )
    )

    summary["standard_error"] = (
        summary["standard_deviation"] / np.sqrt(summary["count"])
    )

    degrees_of_freedom = summary["count"] - 1

    summary["ci95"] = np.where(
        summary["count"] > 1,
        t.ppf(0.975, degrees_of_freedom)
        * summary["standard_error"],
        np.nan,
    )

    return summary


def main() -> None:
    results.set_inputs(str(RESULTS_DIR))

    scalars = results.get_scalars(
        FILTER,
        include_attrs=True,
        include_runattrs=True,
        include_itervars=True,
        include_config_entries=True,
    )

    if scalars.empty:
        raise RuntimeError(f"No scalars matched filter: {FILTER}")

    required_columns = {"runID", "value", PARAMETER_COLUMN}
    missing = required_columns.difference(scalars.columns)

    if missing:
        raise RuntimeError(
            f"Missing required columns: {sorted(missing)}"
        )

    scalars["value"] = pd.to_numeric(
        scalars["value"],
        errors="raise",
    )
    scalars[PARAMETER_COLUMN] = pd.to_numeric(
        scalars[PARAMETER_COLUMN],
        errors="raise",
    )

    duplicates = scalars.duplicated(
        subset=["runID", "module", "name"],
        keep=False,
    )

    if duplicates.any():
        print(
            "Warning: multiple matching scalar rows exist for at least "
            "one run/module/name combination."
        )

    summary = summarize_with_ci95(
        scalars,
        PARAMETER_COLUMN,
    ).sort_values(PARAMETER_COLUMN)

    print(summary.to_string(index=False))

    figure, axis = plt.subplots(figsize=(8, 5))

    axis.errorbar(
        summary[PARAMETER_COLUMN],
        summary["mean"],
        yerr=summary["ci95"],
        marker="o",
        capsize=4,
    )

    axis.set_xlabel("Number of hosts")
    axis.set_ylabel("Mean throughput [bit/s]")
    axis.set_title("Throughput versus network size")
    axis.grid(True, alpha=0.3)

    figure.tight_layout()

    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(OUTPUT_FILE, dpi=200, bbox_inches="tight")
    plt.close(figure)

    print(f"Created {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
```

---

## Aggregating repetitions correctly

Before aggregating, determine:

* Which columns define an experimental condition.
* Which column identifies repetitions.
* Whether multiple modules contribute to the same condition.
* Whether the desired observation is per-run, per-node or global.
* Whether module-level observations should be averaged, summed or kept separate.

An experimental condition might be defined by:

```python
condition_columns = [
    "numHosts",
    "offeredLoad",
    "configuration",
]
```

A safe workflow is:

```python
grouped = frame.groupby(
    condition_columns,
    dropna=False,
)["value"]
```

Do not group only by the x-axis variable when another parameter also varies.

### Check repetition counts

```python
counts = (
    frame.groupby(condition_columns, dropna=False)["runID"]
    .nunique()
    .reset_index(name="run_count")
)

print(counts.to_string(index=False))
```

If conditions have different repetition counts, report that fact.

---

## Confidence intervals

For independent simulation repetitions, compute confidence intervals over one aggregate value per run.

Do not compute a confidence interval by treating every packet sample in a vector as an independent simulation repetition.

Correct conceptual workflow:

1. Obtain one relevant estimate per run.
2. Group estimates by experimental condition.
3. Compute the mean and confidence interval across runs.

For small repetition counts, use the Student t distribution:

```python
from scipy.stats import t

critical_value = t.ppf(0.975, count - 1)
ci95 = critical_value * standard_error
```

When only one repetition exists, a confidence interval is undefined. Do not replace it with zero.

---

## Reducing a vector to one value per run

When a scalar equivalent is not recorded, derive one value from each vector.

Example: mean delay after a warm-up interval.

```python
import numpy as np
import pandas as pd


def vector_means_after(
    vectors: pd.DataFrame,
    start_time: float,
) -> pd.DataFrame:
    records: list[dict[str, object]] = []

    for _, row in vectors.iterrows():
        times = np.asarray(row["vectime"], dtype=float)
        values = np.asarray(row["vecvalue"], dtype=float)

        mask = times >= start_time
        selected = values[mask]

        if selected.size == 0:
            continue

        record = row.to_dict()
        record["value"] = float(np.mean(selected))
        record["sample_count"] = int(selected.size)

        record.pop("vectime", None)
        record.pop("vecvalue", None)

        records.append(record)

    return pd.DataFrame.from_records(records)
```

Make the aggregation definition explicit:

* Arithmetic mean of samples
* Time-weighted mean
* Sum
* Maximum
* Quantile
* Final value
* Rate over an interval

Do not use an arithmetic sample mean for a piecewise-constant signal when a time-weighted mean is required.

---

## Time-weighted mean for piecewise-constant vectors

For a signal whose value remains constant until the next recorded change:

```python
import numpy as np


def time_weighted_mean(
    times: np.ndarray,
    values: np.ndarray,
    start_time: float | None = None,
    end_time: float | None = None,
) -> float:
    times = np.asarray(times, dtype=float)
    values = np.asarray(values, dtype=float)

    if len(times) != len(values):
        raise ValueError("times and values must have equal lengths")

    if len(times) < 2:
        raise ValueError(
            "At least two samples are needed for a time-weighted mean"
        )

    left = times[0] if start_time is None else start_time
    right = times[-1] if end_time is None else end_time

    if right <= left:
        raise ValueError("end_time must be greater than start_time")

    breakpoints = np.concatenate(
        (
            [left],
            times[(times > left) & (times < right)],
            [right],
        )
    )

    interval_starts = breakpoints[:-1]
    durations = np.diff(breakpoints)

    indices = np.searchsorted(
        times,
        interval_starts,
        side="right",
    ) - 1

    if np.any(indices < 0):
        raise ValueError(
            "The vector does not define a value at the interval start"
        )

    interval_values = values[indices]

    return float(
        np.sum(interval_values * durations)
        / np.sum(durations)
    )
```

Use this for queue lengths, state values and other event-driven piecewise-constant signals.

---

## Histogram from vector samples

Use a histogram to study a sample distribution within one run or across explicitly selected runs.

```python
all_values = np.concatenate(
    [
        np.asarray(values, dtype=float)
        for values in vectors["vecvalue"]
    ]
)

figure, axis = plt.subplots(figsize=(8, 5))

axis.hist(
    all_values,
    bins="auto",
    density=True,
)

axis.set_xlabel("End-to-end delay [s]")
axis.set_ylabel("Density")
axis.set_title("End-to-end delay distribution")
```

Before combining vector samples from several runs, decide whether pooling is statistically meaningful.

Pooling gives more weight to runs that produced more samples. That may be undesirable.

When every run should have equal weight, compute per-run summaries or per-run distribution estimates instead.

---

## Empirical cumulative distribution function

Use an ECDF when distributions must be compared without arbitrary histogram-bin choices.

```python
import numpy as np


def ecdf(values: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    sorted_values = np.sort(np.asarray(values, dtype=float))
    probabilities = (
        np.arange(1, len(sorted_values) + 1)
        / len(sorted_values)
    )
    return sorted_values, probabilities
```

Plot:

```python
x_values, probabilities = ecdf(values)

axis.step(
    x_values,
    probabilities,
    where="post",
)

axis.set_xlabel("End-to-end delay [s]")
axis.set_ylabel("Empirical cumulative probability")
axis.set_ylim(0.0, 1.0)
```

State whether samples were:

* Taken from one run.
* Pooled across runs.
* Balanced across runs.
* Aggregated before plotting.

---

## Throughput from cumulative counters

If a vector contains cumulative bytes or bits, derive interval throughput using differences.

```python
delta_time = np.diff(times)
delta_bytes = np.diff(byte_values)

valid = delta_time > 0

throughput_bps = (
    8.0 * delta_bytes[valid] / delta_time[valid]
)
throughput_times = times[1:][valid]
```

Validate that the counter is monotonic. Handle resets explicitly:

```python
valid = (delta_time > 0) & (delta_bytes >= 0)
```

Do not silently interpret a reset as negative throughput.

---

## Handling multiple modules

A query may return one vector or scalar per:

* Host
* Interface
* Application
* Queue
* Radio
* Traffic flow

Do not combine these automatically.

Choose one of the following explicitly:

### Plot every module separately

```python
for _, row in vectors.iterrows():
    axis.plot(
        row["vectime"],
        row["vecvalue"],
        label=row["module"],
    )
```

### Select one module

Use a narrow result filter:

```python
'module =~ "Network.host[0].app[0]"'
```

### Aggregate modules within each run

For scalars:

```python
per_run = (
    scalars.groupby(
        ["runID", "numHosts"],
        as_index=False,
    )["value"]
    .sum()
)
```

Use `sum`, `mean`, `min`, `max` or another operation only when its meaning is justified by the metric.

For example:

* Sum per-node packet counts for a network total.
* Mean per-node latency only if each node should have equal weight.
* Use weighted aggregation when nodes generated different packet counts.

---

## Units

Always inspect and preserve units.

```python
units = vectors["unit"].dropna().unique()
print(units)
```

Do not mix rows with incompatible units.

Convert units deliberately:

```python
values_ms = values_s * 1000.0
```

Update the axis label:

```python
axis.set_ylabel("End-to-end delay [ms]")
```

Common conversions:

```python
seconds_to_milliseconds = 1000.0
bytes_to_bits = 8.0
bits_per_second_to_megabits_per_second = 1e-6
watts_to_milliwatts = 1000.0
```

Never infer a unit from a result name when a recorded `unit` attribute is available.

---

## Plot quality requirements

Every finished plot must have:

* A descriptive title.
* A labeled x-axis.
* A labeled y-axis.
* Units where applicable.
* A legend when several series are shown.
* A sensible plot type.
* A deterministic output filename.
* Adequate image resolution.
* Tight layout.
* Closed Matplotlib figures after saving.

Recommended pattern:

```python
figure.tight_layout()
figure.savefig(
    output_file,
    dpi=200,
    bbox_inches="tight",
)
plt.close(figure)
```

Avoid:

* Unreadable legends with dozens of modules.
* Lines connecting unrelated categorical values.
* Scientific notation when a clearer engineering unit is available.
* Excessively dense raw vector plots.
* Plotting millions of points without reduction.
* Relying on interactive display through `plt.show()` in automated workflows.

---

## Large vector handling

When vectors are large:

1. Apply a narrow result filter.
2. Restrict the simulation-time interval.
3. Select only required modules and result names.
4. Aggregate before plotting when raw detail is unnecessary.
5. Downsample only for visualization, not for statistical computation.
6. State any downsampling performed.

Simple display-only downsampling:

```python
max_points = 10_000

if len(times) > max_points:
    indices = np.linspace(
        0,
        len(times) - 1,
        max_points,
        dtype=int,
    )
    display_times = times[indices]
    display_values = values[indices]
else:
    display_times = times
    display_values = values
```

Do not use this reduced series to compute statistics unless the reduction method is statistically justified.

---

## Reusable command-line plotting template

```python
#!/usr/bin/env python3

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
from omnetpp.scave import results


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot an OMNeT++ output vector."
    )

    parser.add_argument(
        "results_path",
        type=Path,
        help="Directory or result-file pattern",
    )
    parser.add_argument(
        "--name",
        required=True,
        help="Exact or wildcard vector result name",
    )
    parser.add_argument(
        "--module",
        default="**",
        help="OMNeT++ module-path filter",
    )
    parser.add_argument(
        "--start-time",
        type=float,
        default=None,
    )
    parser.add_argument(
        "--end-time",
        type=float,
        default=None,
    )
    parser.add_argument(
        "--kind",
        choices=("line", "step", "scatter"),
        default="line",
    )
    parser.add_argument(
        "--title",
        default=None,
    )
    parser.add_argument(
        "--ylabel",
        default=None,
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
    )

    return parser.parse_args()


def build_filter(module: str, name: str) -> str:
    return (
        f'module =~ "{module}" '
        f'AND name =~ "{name}"'
    )


def crop_vector(
    times: np.ndarray,
    values: np.ndarray,
    start_time: float | None,
    end_time: float | None,
) -> tuple[np.ndarray, np.ndarray]:
    mask = np.ones(len(times), dtype=bool)

    if start_time is not None:
        mask &= times >= start_time

    if end_time is not None:
        mask &= times <= end_time

    return times[mask], values[mask]


def main() -> None:
    arguments = parse_arguments()

    filter_expression = build_filter(
        arguments.module,
        arguments.name,
    )

    results.set_inputs(str(arguments.results_path))

    vectors = results.get_vectors(
        filter_expression,
        include_attrs=True,
        include_runattrs=True,
        include_itervars=True,
        include_config_entries=True,
    )

    if vectors.empty:
        raise RuntimeError(
            f"No vectors matched filter: {filter_expression}"
        )

    print("Matched results:")
    print(
        vectors[["runID", "module", "name"]]
        .drop_duplicates()
        .to_string(index=False)
    )

    figure, axis = plt.subplots(figsize=(10, 5))

    plotted_series = 0

    for _, row in vectors.iterrows():
        times = np.asarray(row["vectime"], dtype=float)
        values = np.asarray(row["vecvalue"], dtype=float)

        if len(times) != len(values):
            raise RuntimeError(
                f"Mismatched arrays in {row['module']} "
                f"for run {row['runID']}."
            )

        times, values = crop_vector(
            times,
            values,
            arguments.start_time,
            arguments.end_time,
        )

        if len(times) == 0:
            continue

        label = f"{row['module']} — {row['runID']}"

        if arguments.kind == "line":
            axis.plot(times, values, label=label)
        elif arguments.kind == "step":
            axis.step(
                times,
                values,
                where="post",
                label=label,
            )
        else:
            axis.scatter(
                times,
                values,
                s=8,
                label=label,
            )

        plotted_series += 1

    if plotted_series == 0:
        raise RuntimeError(
            "All matching vectors were empty after time cropping."
        )

    axis.set_xlabel("Simulation time [s]")
    axis.set_ylabel(arguments.ylabel or arguments.name)
    axis.set_title(arguments.title or arguments.name)
    axis.grid(True, alpha=0.3)

    if plotted_series > 1:
        axis.legend(fontsize="small")

    figure.tight_layout()

    arguments.output.parent.mkdir(
        parents=True,
        exist_ok=True,
    )
    figure.savefig(
        arguments.output,
        dpi=200,
        bbox_inches="tight",
    )
    plt.close(figure)

    print(f"Created {arguments.output}")


if __name__ == "__main__":
    main()
```

Example:

```bash
python analysis/plot_vector.py results \
    --name "queueLength:vector" \
    --module "Network.host[*].wlan[0].mac.queue" \
    --kind step \
    --ylabel "Queue length [packets]" \
    --title "MAC queue length" \
    --output analysis/figures/mac-queue-length.png
```

---

## Agent workflow

When asked to generate plots from OMNeT++/INET results, follow this sequence.

### Step 1: Locate inputs

Identify:

* Result directory or file patterns.
* `.sca` files.
* `.vec` files.
* Relevant `omnetpp.ini`, if available.
* Requested metric.
* Requested experimental comparison.
* Desired output format.

### Step 2: Inspect results

Query broadly enough to discover candidate result names and modules.

Report:

* Candidate result names.
* Candidate module paths.
* Units.
* Number of runs.
* Relevant iteration variables.

### Step 3: Define the measurement precisely

Translate the user's request into:

* Result type: scalar, vector, statistic or histogram.
* Exact filter expression.
* Experimental-condition columns.
* Repetition identifier.
* Warm-up interval.
* Aggregation operation.
* Plot type.
* Axis units.

### Step 4: Validate data

Check:

* Nonempty result.
* Expected modules and names.
* Units.
* Run count.
* Repetition count.
* Missing data.
* Duplicate observations.
* Array lengths.
* Time ordering.
* Whether all conditions are represented.

### Step 5: Transform

Perform only justified operations, such as:

* Unit conversion.
* Warm-up removal.
* Per-run aggregation.
* Network-level aggregation.
* Confidence interval calculation.
* ECDF construction.
* Time-weighted averaging.

### Step 6: Plot

Use a plot that matches the metric's semantics.

### Step 7: Save artifacts

Save:

* The plotting script.
* The generated image.
* Optionally, a compact CSV containing the final aggregated data used in the plot.

The optional CSV is an output of the analysis, not an intermediate replacement for native OMNeT++ loading.

### Step 8: Report assumptions

State:

* Result filter.
* Modules included.
* Runs included.
* Warm-up interval.
* Aggregation method.
* Confidence-interval method.
* Unit conversions.
* Any dropped or missing observations.
* Any downsampling used for display.

---

## Common mistakes to prevent

### Empty query results

Cause:

* Incorrect result name.
* Incorrect module path.
* Wrong wildcard.
* Result recording disabled.
* Searching scalars when the metric is a vector.

Response:

* Run discovery queries.
* Print unique result names and modules.
* Check `omnetpp.ini` recording configuration.

### Mixing configurations

Cause:

* Grouping by one iteration variable while another variable also changes.

Response:

* Inspect all iteration-variable and configuration columns.
* Define the complete condition key.

### Pseudoreplication

Cause:

* Treating packets, vector samples or nodes as independent simulation repetitions.

Response:

* Aggregate to the correct per-run observation first.
* Compute uncertainty across independent runs.

### Pooling samples unevenly

Cause:

* Concatenating vectors from runs with different packet counts.

Response:

* Decide whether observations or runs should receive equal weight.
* Prefer per-run summaries when estimating experiment-level uncertainty.

### Wrong mean for event-driven values

Cause:

* Applying `np.mean()` to queue or state-change samples.

Response:

* Use a time-weighted mean for piecewise-constant signals.

### Misleading time-series lines

Cause:

* Connecting independent per-packet samples.

Response:

* Use scatter points or an ECDF.

### Duplicate scalar matches

Cause:

* A broad filter matches several modules or result variants.

Response:

* Inspect `(runID, module, name)`.
* Narrow the filter or define a justified module aggregation.

### Missing warm-up removal

Cause:

* Including initialization transients in steady-state estimates.

Response:

* Apply the experiment's stated warm-up interval.
* Never invent a warm-up interval without evidence.

### Excessive raw plotting

Cause:

* Plotting too many runs, nodes or vector samples.

Response:

* Select representative series.
* Use summaries or faceting.
* Downsample only for rendering.
* Preserve full data for computations.

---

## Completion criteria

An OMNeT++/INET plotting task is complete only when:

* Native `omnetpp.scave` loading is used.
* The actual result names and module paths have been inspected.
* The filter expression is explicit.
* Empty and malformed data are rejected.
* Experimental conditions and repetitions are correctly identified.
* Aggregation semantics are documented.
* Units are checked and shown.
* The plot type matches the recorded quantity.
* The script runs non-interactively.
* The output image is saved deterministically.
* Relevant assumptions and exclusions are reported.

---

## Reference basis

This skill follows the OMNeT++ result-analysis model in which simulations produce scalar and vector result files and the `omnetpp.scave` Python package exposes result queries as Pandas-compatible data. In OMNeT++ 6.1 and later, the package uses native OMNeT++ result-file readers. The API supports querying vectors, scalars, statistics and histograms and attaching result attributes, run attributes and iteration variables to returned data.
