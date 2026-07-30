#!/usr/bin/env python3
"""Plot validated OMNeT++ output vectors without experiment-specific reduction."""

import argparse
import os
import tempfile
from pathlib import Path

os.environ.setdefault(
    "MPLCONFIGDIR",
    str(Path(tempfile.gettempdir()) / "omnetpp-result-plotting-matplotlib"),
)

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
from omnetpp.scave import results


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "inputs",
        nargs="+",
        help="OMNeT++ result files, directories, or file patterns",
    )
    parser.add_argument(
        "--filter",
        required=True,
        help="OMNeT++ result-selection expression",
    )
    parser.add_argument(
        "--kind",
        choices=("line", "step", "scatter"),
        default="line",
    )
    parser.add_argument("--start-time", type=float)
    parser.add_argument("--end-time", type=float)
    parser.add_argument("--title")
    parser.add_argument("--ylabel")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--max-series",
        type=int,
        default=20,
        help="Reject broader queries instead of drawing unreadable legends",
    )
    parser.add_argument(
        "--max-points",
        type=int,
        help="Display-only uniform downsampling limit per series",
    )
    return parser.parse_args()


def validate_arguments(arguments: argparse.Namespace) -> None:
    if (
        arguments.start_time is not None
        and arguments.end_time is not None
        and arguments.end_time < arguments.start_time
    ):
        raise ValueError("--end-time must not precede --start-time")
    if arguments.max_series < 1:
        raise ValueError("--max-series must be positive")
    if arguments.max_points is not None and arguments.max_points < 2:
        raise ValueError("--max-points must be at least 2")
    if arguments.kind == "step" and arguments.max_points is not None:
        raise ValueError(
            "Uniform downsampling can distort state transitions; "
            "omit --max-points for step plots"
        )


def downsample(
    times: np.ndarray,
    values: np.ndarray,
    max_points: int | None,
) -> tuple[np.ndarray, np.ndarray]:
    if max_points is None or len(times) <= max_points:
        return times, values
    indices = np.linspace(0, len(times) - 1, max_points, dtype=int)
    return times[indices], values[indices]


def main() -> None:
    arguments = parse_arguments()
    validate_arguments(arguments)
    results.set_inputs(arguments.inputs)

    query_options = dict(
        include_attrs=True,
        include_runattrs=True,
        include_itervars=True,
        omit_empty_vectors=True,
    )
    if arguments.start_time is not None:
        query_options["start_time"] = arguments.start_time
    if arguments.end_time is not None:
        query_options["end_time"] = arguments.end_time

    vectors = results.get_vectors(arguments.filter, **query_options)
    if vectors.empty:
        raise RuntimeError(f"No vectors matched: {arguments.filter}")

    required = {"runID", "module", "name", "vectime", "vecvalue"}
    missing = required.difference(vectors.columns)
    if missing:
        raise RuntimeError(f"Missing vector columns: {sorted(missing)}")
    if len(vectors) > arguments.max_series:
        raise RuntimeError(
            f"Query matched {len(vectors)} series; narrow it or raise --max-series"
        )
    names = set(vectors["name"].dropna().astype(str))
    if len(names) != 1:
        raise RuntimeError(
            f"Query matched multiple result names: {sorted(names)}"
        )

    inventory_columns = [
        column
        for column in ("runID", "module", "name", "unit")
        if column in vectors.columns
    ]
    print(vectors[inventory_columns].drop_duplicates().to_string(index=False))

    if "unit" in vectors.columns:
        units = set(vectors["unit"].dropna().astype(str))
        if len(units) > 1:
            raise RuntimeError(f"Query matched incompatible units: {sorted(units)}")

    figure, axis = plt.subplots(figsize=(10, 5))
    downsampled = False

    for index, row in vectors.iterrows():
        times = np.asarray(row["vectime"], dtype=float)
        values = np.asarray(row["vecvalue"], dtype=float)
        if len(times) != len(values) or len(times) == 0:
            raise RuntimeError(f"Malformed or empty vector row {index}")
        if np.any(np.diff(times) < 0):
            raise RuntimeError(f"Non-monotonic timestamps in vector row {index}")

        display_times, display_values = downsample(
            times,
            values,
            arguments.max_points,
        )
        downsampled |= len(display_times) < len(times)
        label = f"{row['module']} — {row['runID']}"

        if arguments.kind == "line":
            axis.plot(display_times, display_values, label=label)
        elif arguments.kind == "step":
            axis.step(display_times, display_values, where="post", label=label)
        else:
            axis.scatter(display_times, display_values, s=8, label=label)

    axis.set_xlabel("Simulation time [s]")
    axis.set_ylabel(arguments.ylabel or str(vectors.iloc[0]["name"]))
    axis.set_title(arguments.title or str(vectors.iloc[0]["name"]))
    axis.grid(True, alpha=0.3)
    if len(vectors) > 1:
        axis.legend(fontsize="small")

    figure.tight_layout()
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(arguments.output, dpi=200, bbox_inches="tight")
    plt.close(figure)

    print(f"Created {arguments.output}")
    if downsampled:
        print("Applied display-only uniform downsampling; computations used no reduction.")


if __name__ == "__main__":
    main()
