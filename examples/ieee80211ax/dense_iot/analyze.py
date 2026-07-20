#!/usr/bin/env python3
"""Analyze and plot the dense-IoT campaign using OMNeT++'s native result API."""

from __future__ import annotations

import argparse
import math
import re
from pathlib import Path
from typing import Iterable

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from omnetpp.scave import results
from scipy.stats import t


EXAMPLE_DIR = Path(__file__).resolve().parent
DEFAULT_RESULTS_DIR = EXAMPLE_DIR / "results"
DEFAULT_OUTPUT_DIR = EXAMPLE_DIR / "analysis"
CONFIG_METADATA = {
    "AxUl": ("AX", "UL"),
    "AcUl": ("AC", "UL"),
    "AxDl": ("AX", "DL"),
    "AcDl": ("AC", "DL"),
    "AxMixed": ("AX", "Mixed"),
    "AcMixed": ("AC", "Mixed"),
}
STATION_COUNTS = {64, 128, 256, 512}
RUNS_PER_STATION_COUNT = 5
MEASUREMENT_START = 20.0
MEASUREMENT_END = 120.0
MEASUREMENT_DURATION = MEASUREMENT_END - MEASUREMENT_START
INITIAL_ENERGY_J = 1000.0
QUERY_OPTIONS = {
    "include_attrs": True,
    "include_runattrs": True,
    "include_itervars": True,
    "include_config_entries": True,
}


def discover_files(results_dir: Path) -> list[str]:
    paths = sorted(results_dir.glob("*.sca")) + sorted(results_dir.glob("*.vec"))
    if not paths:
        raise FileNotFoundError(f"No .sca/.vec files found in {results_dir}")
    return [str(path) for path in paths]


def load_scalars(paths: list[str], name: str) -> pd.DataFrame:
    expression = f'type =~ scalar AND name =~ "{name}"'
    raw = results.read_result_files(paths, filter_expression=expression)
    frame = results.get_scalars(raw, **QUERY_OPTIONS)
    if frame.empty:
        raise RuntimeError(f"No scalars matched: {expression}")
    print_matches("scalar", expression, frame)
    return normalize_metadata(frame)


def load_vectors(paths: list[str], name: str, required: bool = True) -> pd.DataFrame:
    expression = f'type =~ vector AND name =~ "{name}"'
    raw = results.read_result_files(paths, filter_expression=expression)
    frame = results.get_vectors(raw, omit_empty_vectors=True, **QUERY_OPTIONS)
    if frame.empty:
        if required:
            raise RuntimeError(f"No vectors matched: {expression}")
        return frame
    print_matches("vector", expression, frame)
    frame = normalize_metadata(frame)
    for index, row in frame.iterrows():
        times = np.asarray(row.vectime, dtype=float)
        values = np.asarray(row.vecvalue, dtype=float)
        if len(times) != len(values) or np.any(np.diff(times) < 0):
            raise RuntimeError(f"Malformed vector row {index}: {row.module}/{row.name}")
    return frame


def print_matches(kind: str, expression: str, frame: pd.DataFrame) -> None:
    matches = frame[["module", "name"]].drop_duplicates()
    print(f"MATCH {kind}: {len(matches)} module/name pairs; filter={expression}")


def normalize_metadata(frame: pd.DataFrame) -> pd.DataFrame:
    required = {"runID", "configname", "runnumber", "repetition", "numStations"}
    missing = required - set(frame.columns)
    if missing:
        raise RuntimeError(f"Result metadata is missing columns: {sorted(missing)}")
    frame = frame.copy()
    frame = frame[frame.configname.isin(CONFIG_METADATA)].copy()
    if frame.empty:
        raise RuntimeError("The selected files contain none of the dense-IoT configurations")
    frame["numStations"] = pd.to_numeric(frame["numStations"], errors="raise").astype(int)
    frame["runnumber"] = pd.to_numeric(frame["runnumber"], errors="raise").astype(int)
    frame["repetition"] = pd.to_numeric(frame["repetition"], errors="raise").astype(int)
    return frame


def validate_campaign(frame: pd.DataFrame) -> None:
    observed_configs = set(frame.configname)
    if observed_configs != set(CONFIG_METADATA):
        raise RuntimeError(
            f"Expected configurations {sorted(CONFIG_METADATA)}, found {sorted(observed_configs)}"
        )
    for config in CONFIG_METADATA:
        subset = frame[frame.configname == config]
        if set(subset.numStations) != STATION_COUNTS:
            raise RuntimeError(f"{config}: incomplete station-count set")
        counts = subset.groupby("numStations").runID.nunique()
        if set(counts.index) != STATION_COUNTS or not np.all(
            counts.to_numpy() == RUNS_PER_STATION_COUNT
        ):
            raise RuntimeError(
                f"{config}: expected {RUNS_PER_STATION_COUNT} runs per station "
                f"count, found {counts.to_dict()}"
            )


def validate_ax_features(paths: list[str]) -> None:
    expected_runs_per_config = len(STATION_COUNTS) * RUNS_PER_STATION_COUNT
    agreements = load_scalars(paths, "twtAgreementCount")
    agreements = agreements[agreements.configname.str.startswith("Ax")].copy()
    agreements["value"] = pd.to_numeric(agreements.value, errors="raise")
    for config in ("AxUl", "AxDl", "AxMixed"):
        rows = agreements[agreements.configname == config]
        if rows.runID.nunique() != expected_runs_per_config:
            raise RuntimeError(
                f"{config}: expected TWT results from {expected_runs_per_config} runs, "
                f"found {rows.runID.nunique()}"
            )
    for key, rows in agreements.groupby(["runID", "configname", "numStations"]):
        expected = key[2]
        active = int((rows.value == 1).sum())
        if len(rows) != expected or active != expected:
            raise RuntimeError(
                f"{key[1]}/{key[0]}: expected one active TWT agreement for all "
                f"{expected} STAs, found {active} active in {len(rows)} records"
            )

    trigger_rows = load_scalars(paths, "heUlBasicTriggerSent:count")
    trigger_rows["value"] = pd.to_numeric(trigger_rows.value, errors="raise")
    for config in ("AxUl", "AxMixed"):
        rows = trigger_rows[trigger_rows.configname == config]
        if (
            rows.runID.nunique() != expected_runs_per_config
            or (rows.value <= 0).any()
        ):
            raise RuntimeError(f"{config}: every run must contain Basic Trigger frames")

    dl_mu = load_vectors(paths, "heStaId:vector", required=False)
    dl_mu = dl_mu[
        dl_mu.configname.isin(("AxDl", "AxMixed"))
        & dl_mu.module.str.endswith(".ap.wlan[0].radio")
    ]
    if dl_mu.runID.nunique() != 2 * expected_runs_per_config:
        raise RuntimeError(
            "Every AxDl/AxMixed run must contain at least one recorded DL MU transmission"
        )
    print("Validated TWT agreement coverage and UL/DL OFDMA activity.")


def receiver_direction(config: str, module: str) -> str | None:
    if config.endswith("Ul"):
        return "UL" if module.endswith(".server.app[0]") else None
    if config.endswith("Dl"):
        return "DL" if re.search(r"\.sta\[\d+\]\.app\[0\]$", module) else None
    if module.endswith(".server.app[0]"):
        return "UL"
    if re.search(r"\.sta\[\d+\]\.app\[1\]$", module):
        return "DL"
    return None


def add_condition_columns(frame: pd.DataFrame) -> pd.DataFrame:
    frame = frame.copy()
    frame["technology"] = frame.configname.map(lambda value: CONFIG_METADATA[value][0])
    frame["workload"] = frame.configname.map(lambda value: CONFIG_METADATA[value][1])
    frame["direction"] = [
        receiver_direction(config, module)
        for config, module in zip(frame.configname, frame.module)
    ]
    return frame[frame.direction.notna()].copy()


def key_columns() -> list[str]:
    return [
        "runID", "configname", "runnumber", "repetition", "numStations",
        "technology", "workload", "direction",
    ]


def aggregate_application_scalars(frame: pd.DataFrame, value_name: str) -> pd.DataFrame:
    frame = add_condition_columns(frame)
    frame["value"] = pd.to_numeric(frame.value, errors="raise")
    return (
        frame.groupby(key_columns(), as_index=False, dropna=False).value.sum()
        .rename(columns={"value": value_name})
    )


def crop_samples(row: pd.Series) -> np.ndarray:
    times = np.asarray(row.vectime, dtype=float)
    values = np.asarray(row.vecvalue, dtype=float)
    selected = (times >= MEASUREMENT_START) & (times < MEASUREMENT_END)
    return values[selected]


def aggregate_delays(frame: pd.DataFrame) -> pd.DataFrame:
    frame = add_condition_columns(frame)
    records: list[dict[str, object]] = []
    group_columns = key_columns()
    for key, rows in frame.groupby(group_columns, dropna=False):
        samples = [crop_samples(row) for _, row in rows.iterrows()]
        samples = [sample for sample in samples if sample.size]
        if not samples:
            continue
        values = np.concatenate(samples)
        record = dict(zip(group_columns, key))
        record.update(
            mean_delay_s=float(np.mean(values)),
            p95_delay_s=float(np.percentile(values, 95)),
            delay_samples=int(values.size),
        )
        records.append(record)
    if not records:
        raise RuntimeError("No end-to-end delay samples remain in the measurement window")
    return pd.DataFrame.from_records(records)


def offered_packets(workload: str, direction: str, num_stations: int) -> int:
    interval = 10 if workload == "Mixed" and direction == "DL" else 1
    return num_stations * int(MEASUREMENT_DURATION / interval)


def build_performance(paths: list[str]) -> pd.DataFrame:
    byte_rows = aggregate_application_scalars(
        load_scalars(paths, "packetReceived:sum(packetBytes)"), "delivered_bytes"
    )
    count_rows = aggregate_application_scalars(
        load_scalars(paths, "packetReceived:count"), "delivered_packets"
    )
    delay_rows = aggregate_delays(load_vectors(paths, "endToEndDelay:vector"))
    performance = byte_rows.merge(count_rows, on=key_columns(), validate="one_to_one")
    performance = performance.merge(delay_rows, on=key_columns(), validate="one_to_one")
    performance["goodput_bps"] = performance.delivered_bytes * 8 / MEASUREMENT_DURATION
    performance["offered_packets"] = [
        offered_packets(workload, direction, num_stations)
        for workload, direction, num_stations in zip(
            performance.workload, performance.direction, performance.numStations
        )
    ]
    performance["delivery_ratio"] = performance.delivered_packets / performance.offered_packets

    aggregate_records = []
    aggregate_keys = [
        "runID", "configname", "runnumber", "repetition", "numStations",
        "technology", "workload",
    ]
    delay_vectors = add_condition_columns(load_vectors(paths, "endToEndDelay:vector"))
    for key, rows in delay_vectors.groupby(aggregate_keys, dropna=False):
        samples = [crop_samples(row) for _, row in rows.iterrows()]
        samples = [sample for sample in samples if sample.size]
        if not samples:
            continue
        values = np.concatenate(samples)
        record = dict(zip(aggregate_keys, key))
        record.update(
            direction="aggregate",
            mean_delay_s=float(np.mean(values)),
            p95_delay_s=float(np.percentile(values, 95)),
            delay_samples=int(values.size),
        )
        aggregate_records.append(record)
    aggregate_delay = pd.DataFrame.from_records(aggregate_records)
    aggregate_counts = (
        performance.groupby(aggregate_keys, as_index=False)
        .agg(
            delivered_bytes=("delivered_bytes", "sum"),
            delivered_packets=("delivered_packets", "sum"),
            offered_packets=("offered_packets", "sum"),
        )
    )
    aggregate_counts["direction"] = "aggregate"
    aggregate_counts["goodput_bps"] = aggregate_counts.delivered_bytes * 8 / MEASUREMENT_DURATION
    aggregate_counts["delivery_ratio"] = (
        aggregate_counts.delivered_packets / aggregate_counts.offered_packets
    )
    aggregate_performance = aggregate_counts.merge(
        aggregate_delay, on=aggregate_keys + ["direction"], validate="one_to_one"
    )
    return pd.concat([performance, aggregate_performance], ignore_index=True)


def build_energy(paths: list[str]) -> pd.DataFrame:
    frame = load_scalars(paths, "residualEnergyCapacity:last")
    units = set(frame.unit.dropna().astype(str))
    if units != {"J"}:
        raise RuntimeError(f"Expected residual energy in joules, found {sorted(units)}")
    frame = frame[frame.module.str.contains(r"\.sta\[\d+\]\.energyStorage$")].copy()
    frame["value"] = pd.to_numeric(frame.value, errors="raise")
    frame["technology"] = frame.configname.map(lambda value: CONFIG_METADATA[value][0])
    frame["workload"] = frame.configname.map(lambda value: CONFIG_METADATA[value][1])
    group_columns = [
        "runID", "configname", "runnumber", "repetition", "numStations",
        "technology", "workload",
    ]
    counts = frame.groupby(group_columns).size()
    for key, count in counts.items():
        if count != key[4]:
            raise RuntimeError(f"{key[1]}/{key[4]}: expected {key[4]} station energy values, found {count}")
    frame["consumed_energy_j"] = INITIAL_ENERGY_J - frame.value
    return (
        frame.groupby(group_columns, as_index=False)
        .agg(
            mean_station_energy_j=("consumed_energy_j", "mean"),
            total_station_energy_j=("consumed_energy_j", "sum"),
        )
    )


def ci95(values: Iterable[float]) -> tuple[float, float, int]:
    array = np.asarray(list(values), dtype=float)
    if not array.size or np.any(~np.isfinite(array)):
        raise ValueError("Confidence-interval input must be finite and nonempty")
    mean = float(np.mean(array))
    if len(array) == 1:
        return mean, math.nan, 1
    sem = float(np.std(array, ddof=1) / math.sqrt(len(array)))
    return mean, float(t.ppf(0.975, len(array) - 1) * sem), len(array)


def summarize(performance: pd.DataFrame) -> pd.DataFrame:
    records = []
    columns = ["technology", "workload", "numStations", "direction"]
    metrics = ("goodput_bps", "delivery_ratio", "mean_delay_s", "p95_delay_s")
    for key, rows in performance.groupby(columns):
        record = dict(zip(columns, key))
        for metric in metrics:
            mean, interval, count = ci95(rows[metric])
            record[f"{metric}_mean"] = mean
            record[f"{metric}_ci95"] = interval
            record["run_count"] = count
        records.append(record)
    return pd.DataFrame.from_records(records)


def paired_energy_reduction(energy: pd.DataFrame) -> tuple[pd.DataFrame, pd.DataFrame]:
    keys = ["workload", "numStations", "repetition"]
    ax = energy[energy.technology == "AX"][keys + ["mean_station_energy_j"]]
    ac = energy[energy.technology == "AC"][keys + ["mean_station_energy_j"]]
    paired = ax.merge(ac, on=keys, suffixes=("_ax", "_ac"), validate="one_to_one")
    paired["energy_reduction"] = 1 - (
        paired.mean_station_energy_j_ax / paired.mean_station_energy_j_ac
    )
    records = []
    for key, rows in paired.groupby(["workload", "numStations"]):
        mean, interval, count = ci95(rows.energy_reduction)
        records.append(
            {
                "workload": key[0],
                "numStations": key[1],
                "energy_reduction_mean": mean,
                "energy_reduction_ci95": interval,
                "run_count": count,
            }
        )
    return paired, pd.DataFrame.from_records(records)


def inspect_he_mcs(paths: list[str]) -> None:
    frame = load_vectors(paths, "heRateSelectedMcs:vector", required=False)
    if frame.empty:
        print("No HE MCS vectors were recorded.")
        return
    records = []
    for (config, count), rows in frame.groupby(["configname", "numStations"]):
        values = [np.asarray(row.vecvalue, dtype=int) for _, row in rows.iterrows()]
        values = np.concatenate([value for value in values if value.size])
        records.append((config, count, int(values.min()), int(values.max()), len(np.unique(values))))
    print("\nObserved HE MCS range (configuration, STAs, min, max, distinct):")
    for record in records:
        print(" ", record)


def inspect_ac_rates(paths: list[str]) -> None:
    frame = load_vectors(paths, "datarateChanged:vector", required=False)
    if frame.empty:
        print("No AC data-rate vectors were recorded.")
        return
    records = []
    for (config, count), rows in frame.groupby(["configname", "numStations"]):
        values = [np.asarray(row.vecvalue, dtype=float) for _, row in rows.iterrows()]
        values = np.concatenate([value for value in values if value.size]) / 1e6
        records.append(
            (config, count, float(values.min()), float(values.max()), len(np.unique(values)))
        )
    print("\nObserved AC rate range (configuration, STAs, min/max Mbit/s, distinct):")
    for record in records:
        print(" ", record)


def plot_dashboard(summary: pd.DataFrame, reduction: pd.DataFrame, output: Path) -> None:
    workloads = ("UL", "DL", "Mixed")
    colors = {"AX": "#0072B2", "AC": "#D55E00"}
    figure, axes = plt.subplots(3, 3, figsize=(13, 11), sharex="col")
    aggregate = summary[summary.direction == "aggregate"]
    for row_index, workload in enumerate(workloads):
        workload_rows = aggregate[aggregate.workload == workload]
        for technology in ("AX", "AC"):
            rows = workload_rows[workload_rows.technology == technology].sort_values("numStations")
            axes[row_index, 0].errorbar(
                rows.numStations,
                rows.goodput_bps_mean / 1e3,
                yerr=rows.goodput_bps_ci95 / 1e3,
                marker="o", capsize=3, label=technology, color=colors[technology],
            )
            axes[row_index, 1].errorbar(
                rows.numStations,
                rows.p95_delay_s_mean * 1e3,
                yerr=rows.p95_delay_s_ci95 * 1e3,
                marker="o", capsize=3, label=technology, color=colors[technology],
            )
        energy_rows = reduction[reduction.workload == workload].sort_values("numStations")
        axes[row_index, 2].errorbar(
            energy_rows.numStations,
            100 * energy_rows.energy_reduction_mean,
            yerr=100 * energy_rows.energy_reduction_ci95,
            marker="o", capsize=3, color=colors["AX"],
        )
        axes[row_index, 2].axhline(0, color="black", linewidth=0.8)
        axes[row_index, 0].set_ylabel(f"{workload}\nGoodput [kbit/s]")
        axes[row_index, 1].set_ylabel("Run p95 delay [ms]")
        axes[row_index, 2].set_ylabel("AX energy reduction [%]")
        for axis in axes[row_index]:
            axis.grid(alpha=0.3)
            axis.set_xticks(sorted(STATION_COUNTS))
    axes[0, 0].set_title("Aggregate application goodput")
    axes[0, 1].set_title("95th-percentile end-to-end delay")
    axes[0, 2].set_title("Mean per-STA energy vs AC")
    for axis in axes[-1]:
        axis.set_xlabel("Number of stations")
    axes[0, 0].legend()
    axes[0, 1].legend()
    figure.suptitle("Dense IoT: 802.11ax OFDMA + TWT versus 802.11ac")
    figure.tight_layout(rect=(0, 0, 1, 0.97))
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=200, bbox_inches="tight")
    plt.close(figure)
    print(f"Created {output}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results-dir", type=Path, default=DEFAULT_RESULTS_DIR)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    args = parser.parse_args()

    paths = discover_files(args.results_dir)
    performance = build_performance(paths)
    validate_campaign(performance)
    validate_ax_features(paths)
    energy = build_energy(paths)
    summary = summarize(performance)
    paired_reduction, reduction = paired_energy_reduction(energy)
    inspect_he_mcs(paths)
    inspect_ac_rates(paths)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    performance.to_csv(args.output_dir / "per_run_performance.csv", index=False)
    energy.to_csv(args.output_dir / "per_run_energy.csv", index=False)
    summary.to_csv(args.output_dir / "performance_summary.csv", index=False)
    paired_reduction.to_csv(args.output_dir / "per_run_energy_reduction.csv", index=False)
    reduction.to_csv(args.output_dir / "energy_reduction_summary.csv", index=False)
    plot_dashboard(summary, reduction, args.output_dir / "dense_iot_comparison.png")


if __name__ == "__main__":
    main()
