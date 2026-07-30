#!/usr/bin/env python3
"""Inventory OMNeT++ result items before constructing a plotting query."""

import argparse
import os
import tempfile
from pathlib import Path

os.environ.setdefault(
    "MPLCONFIGDIR",
    str(Path(tempfile.gettempdir()) / "omnetpp-result-plotting-matplotlib"),
)

import pandas as pd
from omnetpp.scave import results


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="List result names, modules, units, runs, and metadata columns.",
    )
    parser.add_argument(
        "inputs",
        nargs="+",
        help="OMNeT++ result files, directories, or file patterns",
    )
    parser.add_argument(
        "--filter",
        default="*",
        help="OMNeT++ result-selection expression (default: *)",
    )
    parser.add_argument(
        "--include-config-entries",
        action="store_true",
        help="Attach configuration entries; this may add many columns",
    )
    parser.add_argument(
        "--show-columns",
        action="store_true",
        help="Print every returned DataFrame column",
    )
    return parser.parse_args()


def describe(
    frame: pd.DataFrame,
    result_type: str,
    show_columns: bool,
) -> set[str]:
    print(f"\n{result_type} ({len(frame)} rows)")
    if frame.empty:
        return set()

    identity_columns = [
        column
        for column in ("module", "name", "unit")
        if column in frame.columns
    ]
    if identity_columns:
        inventory = (
            frame[identity_columns]
            .drop_duplicates()
            .sort_values(identity_columns)
        )
        print(inventory.to_string(index=False))

    run_ids = (
        set(frame["runID"].dropna().astype(str))
        if "runID" in frame.columns
        else set()
    )
    print(f"runs: {len(run_ids)}")

    run_columns = [
        column
        for column in (
            "runID",
            "configname",
            "experiment",
            "measurement",
            "replication",
            "repetition",
            "seedset",
            "iterationvars",
        )
        if column in frame.columns
    ]
    if run_columns:
        print(
            frame[run_columns]
            .drop_duplicates()
            .sort_values("runID")
            .to_string(index=False)
        )
    if show_columns:
        print("columns:", ", ".join(map(str, frame.columns)))
    return run_ids


def main() -> None:
    arguments = parse_arguments()
    results.set_inputs(arguments.inputs)

    query_options = {
        "include_attrs": True,
        "include_runattrs": True,
        "include_itervars": True,
        "include_config_entries": arguments.include_config_entries,
    }
    frames = (
        ("Scalars", results.get_scalars(arguments.filter, **query_options)),
        ("Vectors", results.get_vectors(arguments.filter, **query_options)),
        ("Statistics", results.get_statistics(arguments.filter, **query_options)),
        ("Histograms", results.get_histograms(arguments.filter, **query_options)),
    )

    all_run_ids: set[str] = set()
    for result_type, frame in frames:
        all_run_ids.update(describe(frame, result_type, arguments.show_columns))

    print(f"\nAll matched runs ({len(all_run_ids)}):")
    for run_id in sorted(all_run_ids):
        print(run_id)


if __name__ == "__main__":
    main()
