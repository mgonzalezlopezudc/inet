#!/usr/bin/env python3
"""Create didactical plots from IEEE 802.11ax OMNeT++ result files."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from omnetpp.scave import results


QUERY_OPTIONS = {
    "include_attrs": True,
    "include_runattrs": True,
    "include_itervars": True,
    "include_config_entries": True,
}
MAX_SERIES = 8


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path, help="Result directory or file")
    parser.add_argument(
        "--output-dir", type=Path, default=Path("analysis/figures")
    )
    parser.add_argument(
        "--suite",
        choices=("auto", "common", "rate", "bsr", "twt"),
        default="auto",
    )
    parser.add_argument("--inspect", action="store_true")
    return parser.parse_args()


def load(path: Path):
    if not path.exists():
        raise FileNotFoundError(path)
    pattern = str(path)
    results.set_inputs(pattern)
    return pattern


def get_vectors(input_pattern: str, name: str) -> pd.DataFrame:
    expression = f'name =~ "*{name}*"'
    try:
        frame = results.get_vectors(expression, **QUERY_OPTIONS)
    except ValueError as error:
        if "duplicate entries" not in str(error):
            raise
        fallback_options = dict(QUERY_OPTIONS)
        fallback_options["include_attrs"] = False
        print(f"WARN {name}: duplicate result attributes; retrying without item attributes")
        frame = results.get_vectors(expression, **fallback_options)
    if frame.empty:
        return frame
    required = {"runID", "module", "name", "vectime", "vecvalue"}
    missing = required - set(frame.columns)
    if missing:
        raise RuntimeError(f"{name}: missing columns {sorted(missing)}")
    valid_rows = []
    for index, row in frame.iterrows():
        times = np.asarray(row["vectime"], dtype=float)
        values = np.asarray(row["vecvalue"], dtype=float)
        if len(times) != len(values):
            raise RuntimeError(f"{name}: mismatched arrays in row {index}")
        if len(times) and np.any(np.diff(times) < 0):
            raise RuntimeError(f"{name}: non-monotonic time in row {index}")
        if len(times):
            valid_rows.append(index)
    return frame.loc[valid_rows].copy()


def describe_matches(frame: pd.DataFrame, name: str) -> None:
    if frame.empty:
        print(f"SKIP {name}: no nonempty vectors")
        return
    columns = [column for column in ("runID", "module", "name", "unit") if column in frame]
    print(f"MATCH {name}: {len(frame)} vector(s)")
    print(frame[columns].drop_duplicates().to_string(index=False))


def node_label(module: str) -> str:
    parts = module.split(".")
    for part in parts:
        if part.startswith(("host[", "sta", "ap", "server")):
            suffix = ".".join(parts[-2:])
            return part if suffix.endswith(part) else f"{part}: {suffix}"
    return module


def select_series(frame: pd.DataFrame, limit: int = MAX_SERIES) -> pd.DataFrame:
    ranked = frame.copy()
    ranked["_samples"] = ranked["vecvalue"].map(len)
    return ranked.sort_values(["_samples", "module"], ascending=[False, True]).head(limit)


def save(figure: plt.Figure, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.tight_layout()
    figure.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(figure)
    print(f"CREATED {path}")


def plot_delay_ecdf(raw, output: Path) -> bool:
    name = "endToEndDelay:vector"
    frame = get_vectors(raw, name)
    describe_matches(frame, name)
    if frame.empty:
        return False
    figure, axis = plt.subplots(figsize=(8, 5))
    for _, row in select_series(frame).iterrows():
        values = np.sort(np.asarray(row["vecvalue"], dtype=float) * 1e3)
        probability = np.arange(1, len(values) + 1) / len(values)
        axis.step(values, probability, where="post", label=node_label(row["module"]))
    axis.set(xlabel="End-to-end delay [ms]", ylabel="Empirical cumulative probability", title="Packet delay distribution")
    axis.set_ylim(0, 1)
    axis.grid(True, alpha=0.3)
    axis.legend(fontsize="small")
    save(figure, output / "delay-ecdf.png")
    return True


def plot_queue(raw, output: Path) -> bool:
    name = "queueLength:vector"
    frame = get_vectors(raw, name)
    describe_matches(frame, name)
    if frame.empty:
        return False
    figure, axis = plt.subplots(figsize=(10, 5))
    for _, row in select_series(frame).iterrows():
        axis.step(row["vectime"], row["vecvalue"], where="post", linewidth=1, label=node_label(row["module"]))
    axis.set(xlabel="Simulation time [s]", ylabel="Queued packets", title="Event-driven queue occupancy")
    axis.grid(True, alpha=0.3)
    axis.legend(fontsize="small", ncol=2)
    save(figure, output / "queue-occupancy.png")
    return True


def plot_rate(raw, output: Path) -> bool:
    frames = {name: get_vectors(raw, name) for name in (
        "heRateSelectedMcs:vector", "heRateSelectedNss:vector",
        "datarateSelected:vector", "heRateSuccessProbability:vector"
    )}
    for name, frame in frames.items():
        describe_matches(frame, name)
    available = [name for name, frame in frames.items() if not frame.empty]
    if not available:
        return False
    labels = {
        "heRateSelectedMcs:vector": ("Selected MCS", "step"),
        "heRateSelectedNss:vector": ("Spatial streams (NSS)", "step"),
        "datarateSelected:vector": ("Selected data rate [bit/s]", "scatter"),
        "heRateSuccessProbability:vector": ("Estimated success probability", "scatter"),
    }
    figure, axes = plt.subplots(len(available), 1, figsize=(11, 2.8 * len(available)), sharex=True, squeeze=False)
    axes = axes[:, 0]
    specifications = [(name, *labels[name]) for name in available]
    for axis, (name, ylabel, kind) in zip(axes, specifications):
        frame = select_series(frames[name], 6)
        for _, row in frame.iterrows():
            label = node_label(row["module"])
            if kind == "step":
                axis.step(row["vectime"], row["vecvalue"], where="post", label=label)
            else:
                axis.scatter(row["vectime"], row["vecvalue"], s=8, alpha=0.65, label=label)
        axis.set_ylabel(ylabel)
        axis.grid(True, alpha=0.3)
    axes[0].set_title("HE Minstrel decisions and confidence over time")
    axes[-1].set_xlabel("Simulation time [s]")
    axes[0].legend(fontsize="small", ncol=3)
    save(figure, output / "rate-adaptation-timeline.png")
    return True


def plot_bsr(raw, output: Path) -> bool:
    reported = get_vectors(raw, "heUlBufferStatusReportedBytes:vector")
    scheduled = get_vectors(raw, "heUlBufferStatusScheduledBytes:vector")
    describe_matches(reported, "heUlBufferStatusReportedBytes:vector")
    describe_matches(scheduled, "heUlBufferStatusScheduledBytes:vector")
    if reported.empty or scheduled.empty:
        return False
    figure, axes = plt.subplots(2, 1, figsize=(11, 7), sharex=True)
    for axis, frame, title in (
        (axes[0], reported, "Backlog reported by stations"),
        (axes[1], scheduled, "Backlog selected for scheduling"),
    ):
        for _, row in select_series(frame, 6).iterrows():
            axis.step(row["vectime"], row["vecvalue"], where="post", label=node_label(row["module"]))
        axis.set_ylabel("Bytes")
        axis.set_title(title)
        axis.grid(True, alpha=0.3)
    axes[-1].set_xlabel("Simulation time [s]")
    axes[0].legend(fontsize="small", ncol=3)
    save(figure, output / "bsr-reported-vs-scheduled.png")
    return True


def plot_twt(raw, output: Path) -> bool:
    frame = get_vectors(raw, "radioMode:vector")
    describe_matches(frame, "radioMode:vector")
    if frame.empty:
        return False
    selected = select_series(frame, 6)
    start = min(float(np.min(row["vectime"])) for _, row in selected.iterrows())
    end = max(float(np.max(row["vectime"])) for _, row in selected.iterrows())
    sample_times = np.linspace(start, end, 2000)
    raster = []
    labels = []
    for _, row in selected.iterrows():
        times = np.asarray(row["vectime"], dtype=float)
        values = np.asarray(row["vecvalue"], dtype=float)
        indices = np.searchsorted(times, sample_times, side="right") - 1
        indices = np.clip(indices, 0, len(values) - 1)
        raster.append(values[indices])
        labels.append(node_label(row["module"]))
    figure, axis = plt.subplots(figsize=(11, 4.5))
    image = axis.imshow(np.asarray(raster), aspect="auto", interpolation="nearest", extent=(start, end, len(raster) - 0.5, -0.5), cmap="viridis")
    axis.set_yticks(range(len(labels)), labels)
    axis.set(xlabel="Simulation time [s]", ylabel="Radio", title="TWT radio-mode timeline")
    colorbar = figure.colorbar(image, ax=axis, pad=0.02)
    colorbar.set_label("Recorded radio mode code")
    save(figure, output / "twt-radio-mode-timeline.png")
    return True


def inspect(input_pattern: str) -> None:
    for kind, getter in (("scalars", results.get_scalars), ("vectors", results.get_vectors), ("statistics", results.get_statistics), ("histograms", results.get_histograms)):
        frame = getter("*", **QUERY_OPTIONS)
        print(f"\n{kind.upper()} ({len(frame)})")
        if not frame.empty:
            columns = [column for column in ("module", "name", "unit") if column in frame]
            print(frame[columns].drop_duplicates().sort_values(columns).to_string(index=False))


def main() -> None:
    arguments = parse_args()
    raw = load(arguments.results)
    if arguments.inspect:
        inspect(raw)
    generated = []
    if arguments.suite in ("auto", "common"):
        generated.extend((plot_delay_ecdf(raw, arguments.output_dir), plot_queue(raw, arguments.output_dir)))
    if arguments.suite == "rate" or (arguments.suite == "auto" and "he_rate_adaptation" in raw):
        generated.append(plot_rate(raw, arguments.output_dir))
    if arguments.suite == "bsr" or (arguments.suite == "auto" and "he_bsr" in raw):
        generated.append(plot_bsr(raw, arguments.output_dir))
    if arguments.suite == "twt" or (arguments.suite == "auto" and "/twt/" in raw):
        generated.append(plot_twt(raw, arguments.output_dir))
    if not any(generated):
        raise RuntimeError("No requested plot had all required result vectors")


if __name__ == "__main__":
    main()
