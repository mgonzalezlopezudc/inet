#!/usr/bin/env python3
"""Manifest-driven plots and invariants for the IEEE 802.11ax examples."""

from __future__ import annotations

import math
from pathlib import Path
from typing import Any, Callable

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from analysis_core import (
    Condition,
    MeasurementWindow,
    crop_vector,
    jain,
    per_run_delay_percentile,
    per_run_goodput,
    per_run_node_goodput,
    summarize_ci95,
    time_weighted_integral,
    validate_disjoint_streams,
    write_provenance,
)


TRANSMISSION_STATE_TRANSMITTING = 2
RADIO_MODE_NAMES = {
    0: "off",
    1: "sleep",
    2: "receiver",
    3: "transmitter",
    4: "transceiver",
    5: "switching",
}


def save(fig: plt.Figure, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(output, dpi=200, bbox_inches="tight")
    plt.close(fig)
    print(f"CREATED {output}")


def summary(frame: pd.DataFrame, column: str) -> dict[str, float | int]:
    return summarize_ci95(frame[column])


def bar_with_ci(
    axis: plt.Axes,
    labels: list[str],
    frames: list[pd.DataFrame],
    column: str,
    *,
    scale: float = 1.0,
) -> None:
    summaries = [summary(frame, column) for frame in frames]
    values = [float(item["mean"]) * scale for item in summaries]
    errors = [float(item["ci95"]) * scale for item in summaries]
    yerr = None if any(math.isnan(value) for value in errors) else errors
    axis.bar(labels, values, yerr=yerr, capsize=4)
    axis.tick_params(axis="x", rotation=24)
    axis.grid(axis="y", alpha=0.3)


def representative_run(frame: pd.DataFrame) -> pd.DataFrame:
    run_number = int(pd.to_numeric(frame.runnumber).min())
    return frame[pd.to_numeric(frame.runnumber).astype(int) == run_number]


def plot_fragmentation(conditions: list[Condition], output: Path) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(13, 4.8))
    airtime_frames: list[pd.DataFrame] = []
    all_frame_types: set[int] = set()
    for condition in conditions:
        sizes = condition.vectors(
            "packetSentToPeer:vector(packetBytes)",
            module="**.host[*].wlan[0].mac.hcf",
        )
        exemplar = representative_run(sizes)
        values = np.concatenate([
            crop_vector(row.vectime, row.vecvalue, condition.measurement)[1]
            for _, row in exemplar.iterrows()
        ])
        values = np.sort(values)
        axes[0].step(
            values,
            np.arange(1, len(values) + 1) / len(values),
            where="post",
            label=f"{condition.label} (run 0)",
        )

        types = condition.vectors(
            "acknowledgmentFrameType:vector",
            module="**.radio",
        )
        airtimes = condition.vectors(
            "acknowledgmentAirtime:vector",
            module="**.radio",
            expected_unit="s",
        )
        records: list[dict[str, Any]] = []
        for run_id in sorted(set(types.runID) & set(airtimes.runID)):
            type_rows = types[types.runID == run_id]
            airtime_rows = airtimes[airtimes.runID == run_id]
            common_modules = set(type_rows.module) & set(airtime_rows.module)
            if not common_modules:
                raise RuntimeError(f"{condition.config}/{run_id}: no aligned ACK telemetry")
            by_type: dict[int, float] = {}
            for module in common_modules:
                type_row = type_rows[type_rows.module == module].iloc[0]
                airtime_row = airtime_rows[airtime_rows.module == module].iloc[0]
                if not np.array_equal(type_row.vectime, airtime_row.vectime):
                    raise RuntimeError(f"{condition.config}/{module}: ACK telemetry timestamps differ")
                for frame_type, airtime in zip(type_row.vecvalue, airtime_row.vecvalue):
                    frame_type = int(frame_type)
                    by_type[frame_type] = by_type.get(frame_type, 0.0) + float(airtime)
                    all_frame_types.add(frame_type)
            record: dict[str, Any] = {"runID": run_id}
            record.update({f"type_{key}": value for key, value in by_type.items()})
            records.append(record)
        airtime_frames.append(pd.DataFrame.from_records(records))

    axes[0].set(
        xlabel="Transmitted MAC frame size [bytes]",
        ylabel="ECDF",
        title="MAC frame-size distribution (representative run)",
    )
    axes[0].grid(alpha=0.3)
    axes[0].legend(fontsize="small")

    frame_names = {0x1D: "ACK", 0x18: "Block Ack Request", 0x19: "Block Ack"}
    labels = [condition.label for condition in conditions]
    x = np.arange(len(labels))
    width = 0.8 / max(1, len(all_frame_types))
    for index, frame_type in enumerate(sorted(all_frame_types)):
        values = []
        errors = []
        for frame in airtime_frames:
            column = f"type_{frame_type}"
            series = frame[column].fillna(0) if column in frame else pd.Series([0.0] * len(frame))
            item = summarize_ci95(series)
            values.append(float(item["mean"]) * 1e3)
            errors.append(float(item["ci95"]) * 1e3)
        yerr = None if any(math.isnan(value) for value in errors) else errors
        axes[1].bar(
            x + index * width,
            values,
            width,
            yerr=yerr,
            capsize=3,
            label=frame_names.get(frame_type, f"type {frame_type}"),
        )
    axes[1].set_xticks(x + width * (len(all_frame_types) - 1) / 2, labels)
    axes[1].tick_params(axis="x", rotation=24)
    axes[1].set(
        ylabel="Acknowledgment airtime [ms]",
        title="Acknowledgment airtime by frame type",
    )
    axes[1].grid(axis="y", alpha=0.3)
    axes[1].legend(fontsize="small")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.host[*].wlan[0].mac.hcf", "name": "packetSentToPeer:vector(packetBytes)", "value_semantics": "bytes from packetBytes recorder"},
            {"type": "vector", "module": "**.radio", "name": "acknowledgmentFrameType:vector"},
            {"type": "vector", "module": "**.radio", "name": "acknowledgmentAirtime:vector", "unit": "s"},
        ],
        aggregation={"frame_sizes": "ECDF from run 0", "airtime": "per-run sums with 95% t CI"},
    )


def plot_uora(conditions: list[Condition], output: Path) -> None:
    successes: list[pd.DataFrame] = []
    failures: list[pd.DataFrame] = []
    fairness: list[pd.DataFrame] = []
    for condition in conditions:
        attempts = condition.scalars("heUlRandomAccessAttempt:count")
        success = condition.scalars("heUlRandomAccessSuccess:count")
        attempts = attempts[attempts.module.str.contains(".host[", regex=False)]
        success = success[success.module.str.contains(".host[", regex=False)]
        attempt_totals = attempts.groupby("runID", as_index=False).value.sum().rename(columns={"value": "attempts"})
        success_totals = success.groupby("runID", as_index=False).value.sum().rename(columns={"value": "successes"})
        merged = attempt_totals.merge(success_totals, on="runID", validate="one_to_one")
        if (merged.attempts <= 0).any():
            raise RuntimeError(f"{condition.config}: UORA produced no attempts")
        merged["probability"] = merged.successes / merged.attempts
        merged["unsuccessful"] = merged.attempts - merged.successes
        if (merged.successes <= 0).all():
            raise RuntimeError(f"{condition.config}: UORA produced no successful transmissions")
        successes.append(merged[["runID", "probability"]])
        failures.append(merged[["runID", "unsuccessful"]])
        fairness_records = []
        for run_id, rows in success.groupby("runID"):
            fairness_records.append({"runID": run_id, "fairness": jain(rows.value)})
        fairness.append(pd.DataFrame.from_records(fairness_records))
    labels = [condition.label for condition in conditions]
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.8))
    bar_with_ci(axes[0], labels, successes, "probability")
    bar_with_ci(axes[1], labels, failures, "unsuccessful")
    bar_with_ci(axes[2], labels, fairness, "fairness")
    axes[0].set_ylabel("UORA success probability")
    axes[0].set_ylim(0, 1.05)
    axes[1].set_ylabel("Unsuccessful attempts per run")
    axes[2].set_ylabel("Jain fairness of per-STA successes")
    axes[2].set_ylim(0, 1.05)
    fig.suptitle("UORA load and random-access RU comparison")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "scalar", "name": "heUlRandomAccessAttempt:count"},
            {"type": "scalar", "name": "heUlRandomAccessSuccess:count"},
        ],
        aggregation={"observation": "one value per run", "uncertainty": "95% Student-t CI"},
    )


def _energy_per_run(condition: Condition) -> pd.DataFrame:
    power = condition.vectors(
        "powerConsumption:vector",
        module="**.sta[*].wlan[0].radio.energyConsumer",
        expected_unit="W",
    )
    delivered = per_run_goodput(condition)
    records = []
    for run_id, rows in power.groupby("runID"):
        energy_j = sum(
            time_weighted_integral(row.vectime, row.vecvalue, condition.measurement)
            for _, row in rows.iterrows()
        )
        delivered_row = delivered[delivered.runID == run_id]
        if len(delivered_row) != 1 or delivered_row.iloc[0].delivered_bytes <= 0:
            raise RuntimeError(f"{condition.config}/{run_id}: no delivered bits for energy efficiency")
        bits = float(delivered_row.iloc[0].delivered_bytes) * 8
        runnumber = int(rows.iloc[0].runnumber)
        records.append({"runID": run_id, "runnumber": runnumber, "energy_j": energy_j, "energy_per_bit": energy_j / bits, "delivered_bytes": bits / 8})
    return pd.DataFrame.from_records(records)


def plot_twt(conditions: list[Condition], output: Path) -> None:
    if len(conditions) != 2:
        raise RuntimeError("TWT analysis requires baseline and TWT conditions")
    energies = [_energy_per_run(condition) for condition in conditions]
    baseline_delivery = energies[0].set_index("runnumber").delivered_bytes
    twt_delivery = energies[1].set_index("runnumber").delivered_bytes
    common = baseline_delivery.index.intersection(twt_delivery.index)
    if len(common) != conditions[0].expected_repetitions:
        raise RuntimeError("TWT baseline and treatment runs do not align")
    ratios = twt_delivery.loc[common].to_numpy() / baseline_delivery.loc[common].to_numpy()
    threshold = float(conditions[1].condition_metadata.get("minimum_delivery_ratio", 0.95))
    if np.any(ratios < threshold):
        raise RuntimeError(f"TWT delivery ratio below {threshold:.0%}: {ratios}")

    modes = conditions[1].vectors("radioMode:vector", module="**.radio")
    exemplar = representative_run(modes)
    grid = np.linspace(conditions[1].measurement.start, conditions[1].measurement.end, 2500)
    raster = []
    labels = []
    for _, row in exemplar.iterrows():
        indices = np.searchsorted(np.asarray(row.vectime), grid, side="right") - 1
        if np.any(indices < 0):
            continue
        raster.append(np.asarray(row.vecvalue, dtype=float)[indices])
        labels.append(row.module.split(".wlan[")[0].split(".")[-1])
    if not raster:
        raise RuntimeError("TWT radio-mode raster has no usable rows")

    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    image = axes[0].imshow(
        raster,
        aspect="auto",
        interpolation="nearest",
        extent=(grid[0], grid[-1], len(raster) - 0.5, -0.5),
        cmap="viridis",
        vmin=0,
        vmax=5,
    )
    axes[0].set_yticks(range(len(labels)), labels)
    axes[0].set(xlabel="Simulation time [s]", title="TWT radio modes (run 0)")
    colorbar = fig.colorbar(image, ax=axes[0], ticks=sorted(RADIO_MODE_NAMES))
    colorbar.ax.set_yticklabels([RADIO_MODE_NAMES[key] for key in sorted(RADIO_MODE_NAMES)])
    labels = [condition.label for condition in conditions]
    bar_with_ci(axes[1], labels, energies, "energy_per_bit")
    axes[1].set_ylabel("Energy per delivered bit [J/bit]")
    axes[1].set_title(f"TWT delivery ≥ {threshold:.0%} of baseline")
    fig.suptitle("TWT sleep scheduling and energy efficiency")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.radio", "name": "radioMode:vector"},
            {"type": "vector", "module": "**.sta[*].wlan[0].radio.energyConsumer", "name": "powerConsumption:vector", "unit": "W"},
            {"type": "vector", "module": "**.app[*]", "name": "packetReceived:vector(packetBytes)", "unit": "B"},
        ],
        aggregation={"energy": "time-weighted integral per run", "delivery_threshold": threshold, "uncertainty": "95% Student-t CI"},
    )


def plot_rate(conditions: list[Condition], output: Path) -> None:
    if len(conditions) != 1:
        raise RuntimeError("Rate timeline requires one representative condition")
    condition = conditions[0]
    names = [
        ("heRateSelectedMcs:vector", "MCS"),
        ("heRateSelectedNss:vector", "NSS"),
        ("heRateSuccessProbability:vector", "Success probability"),
        ("heRateTxSuccess:vector", "TX success"),
    ]
    frames = [
        condition.vectors(name, module="**.rateControl")
        for name, _ in names
    ]
    selected_mcs = np.concatenate([
        crop_vector(row.vectime, row.vecvalue, condition.measurement)[1]
        for _, row in frames[0].iterrows()
    ])
    if len(np.unique(selected_mcs)) < 2:
        raise RuntimeError(
            f"{condition.config}: rate controller selected only one MCS in the measurement window"
        )
    fig, axes = plt.subplots(len(frames), 1, figsize=(11, 10), sharex=True)
    for axis, frame, (_, ylabel) in zip(axes, frames, names):
        exemplar = representative_run(frame)
        for _, row in exemplar.iterrows():
            times, values = crop_vector(row.vectime, row.vecvalue, condition.measurement)
            axis.scatter(times, values, s=8, alpha=0.7, label=row.module)
        axis.set_ylabel(ylabel)
        axis.grid(alpha=0.3)
    axes[0].legend(fontsize="x-small")
    axes[-1].set_xlabel("Simulation time [s]")
    axes[0].set_title(f"{condition.label}: HE rate decisions and outcomes (run 0)")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[{"type": "vector", "module": "**.rateControl", "name": name} for name, _ in names],
        aggregation={"timeline": "representative run 0; no cross-peer inference"},
    )


def _ap_overlap(condition: Condition) -> pd.DataFrame:
    frame = condition.vectors("transmissionState:vector", module="**.ap*.wlan[0].radio")
    records = []
    for run_id, rows in frame.groupby("runID"):
        if len(rows) != 2:
            raise RuntimeError(f"{condition.config}/{run_id}: expected exactly two AP radios")
        grid = np.linspace(condition.measurement.start, condition.measurement.end, 10000)
        active = []
        for _, row in rows.iterrows():
            indices = np.searchsorted(np.asarray(row.vectime), grid, side="right") - 1
            if np.any(indices < 0):
                raise RuntimeError(f"{condition.config}/{run_id}: AP state starts after measurement")
            active.append(np.asarray(row.vecvalue, dtype=int)[indices] == TRANSMISSION_STATE_TRANSMITTING)
        records.append({"runID": run_id, "overlap": float(np.mean(active[0] & active[1]))})
    return pd.DataFrame.from_records(records)


def _per_run_fairness(condition: Condition, normalized: bool = False) -> pd.DataFrame:
    frame = per_run_node_goodput(condition)
    offered = condition.condition_metadata.get("offered_bps_by_index")
    records = []
    for run_id, rows in frame.groupby("runID"):
        values = rows.goodput_bps.to_numpy(dtype=float)
        if normalized:
            if offered is None or len(offered) != len(values):
                raise RuntimeError(f"{condition.config}: missing offered-load normalization")
            values = values / np.asarray(offered, dtype=float)
        records.append({"runID": run_id, "fairness": jain(values)})
    return pd.DataFrame.from_records(records)


def plot_bss(conditions: list[Condition], output: Path) -> None:
    goodputs = [per_run_goodput(condition) for condition in conditions]
    fairness = [_per_run_fairness(condition) for condition in conditions]
    overlap = [_ap_overlap(condition) for condition in conditions]
    labels = [condition.label for condition in conditions]
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.8))
    bar_with_ci(axes[0], labels, goodputs, "goodput_bps", scale=1e-6)
    bar_with_ci(axes[1], labels, fairness, "fairness")
    bar_with_ci(axes[2], labels, overlap, "overlap", scale=100)
    axes[0].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[1].set_ylabel("Jain fairness")
    axes[1].set_ylim(0, 1.05)
    axes[2].set_ylabel("Concurrent AP transmit time [%]")
    fig.suptitle("BSS coloring and OBSS/PD spatial reuse")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.app[*]", "name": "packetReceived:vector(packetBytes)", "unit": "B"},
            {"type": "vector", "module": "**.ap*.wlan[0].radio", "name": "transmissionState:vector", "transmitting_code": TRANSMISSION_STATE_TRANSMITTING},
        ],
        aggregation={"observation": "per-run measurement-window aggregate", "uncertainty": "95% Student-t CI"},
    )


def plot_width(conditions: list[Condition], output: Path) -> None:
    goodputs = [per_run_goodput(condition) for condition in conditions]
    spectral = []
    for condition, frame in zip(conditions, goodputs):
        bandwidth_hz = float(condition.condition_metadata["bandwidth_mhz"]) * 1e6
        converted = frame[["runID"]].copy()
        converted["efficiency"] = frame.goodput_bps / bandwidth_hz
        spectral.append(converted)
    labels = [condition.label for condition in conditions]
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.8))
    bar_with_ci(axes[0], labels, goodputs, "goodput_bps", scale=1e-6)
    bar_with_ci(axes[1], labels, spectral, "efficiency")
    for condition in conditions:
        delay = condition.vectors("endToEndDelay:vector", module="**.app[*]", expected_unit="s")
        exemplar = representative_run(delay)
        samples = np.concatenate([
            crop_vector(row.vectime, row.vecvalue, condition.measurement)[1]
            for _, row in exemplar.iterrows()
        ]) * 1e3
        samples.sort()
        axes[2].step(samples, np.arange(1, len(samples) + 1) / len(samples), where="post", label=f"{condition.label}, run 0")
    axes[0].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[1].set_ylabel("Goodput spectral efficiency [bit/s/Hz]")
    axes[2].set(xlabel="End-to-end delay [ms]", ylabel="ECDF")
    axes[2].legend(fontsize="small")
    for axis in axes:
        axis.grid(alpha=0.3)
    fig.suptitle("Saturated HE channel-width scaling")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.app[*]", "name": "packetReceived:vector(packetBytes)", "unit": "B"},
            {"type": "vector", "module": "**.app[*]", "name": "endToEndDelay:vector", "unit": "s"},
        ],
        aggregation={"bars": "per-run values with 95% Student-t CI", "ECDF": "representative run 0"},
    )


def plot_dl(conditions: list[Condition], output: Path) -> None:
    workloads = ["symmetric", "asymmetric"]
    fig, axes = plt.subplots(2, 3, figsize=(15, 9))
    for row_index, workload in enumerate(workloads):
        selected = [condition for condition in conditions if condition.condition_metadata.get("workload") == workload]
        labels = [condition.label for condition in selected]
        goodputs = [per_run_goodput(condition) for condition in selected]
        delays = [per_run_delay_percentile(condition, 95) for condition in selected]
        fairness = [_per_run_fairness(condition, normalized=(workload == "asymmetric")) for condition in selected]
        bar_with_ci(axes[row_index, 0], labels, goodputs, "goodput_bps", scale=1e-6)
        bar_with_ci(axes[row_index, 1], labels, delays, "delay_s", scale=1e3)
        bar_with_ci(axes[row_index, 2], labels, fairness, "fairness")
        axes[row_index, 0].set_ylabel(f"{workload.title()} goodput [Mbit/s]")
        axes[row_index, 1].set_ylabel("95th-percentile delay [ms]")
        axes[row_index, 2].set_ylabel("Normalized Jain fairness" if workload == "asymmetric" else "Jain fairness")
        axes[row_index, 2].set_ylim(0, 1.05)
    fig.suptitle("Downlink schedulers under controlled workloads")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.app[*]", "name": "packetReceived:vector(packetBytes)", "unit": "B"},
            {"type": "vector", "module": "**.app[*]", "name": "endToEndDelay:vector", "unit": "s"},
        ],
        aggregation={"observation": "one per run", "delay": "pooled packet p95 within run", "uncertainty": "95% Student-t CI"},
    )


def plot_bsr(conditions: list[Condition], output: Path) -> None:
    fig, axes = plt.subplots(len(conditions), 1, figsize=(12, 4 * len(conditions)), sharex=True)
    axes = np.atleast_1d(axes)
    for axis, condition in zip(axes, conditions):
        reported = representative_run(condition.vectors("heUlBufferStatusReportedBytes:vector"))
        scheduled = representative_run(condition.vectors("heUlBufferStatusScheduledBytes:vector"))
        for _, row in reported.iterrows():
            times, values = crop_vector(row.vectime, row.vecvalue, condition.measurement)
            axis.step(times, values, where="post", label="reported", alpha=0.8)
        for _, row in scheduled.iterrows():
            times, values = crop_vector(row.vectime, row.vecvalue, condition.measurement)
            axis.step(times, values, where="post", label="scheduled", alpha=0.8)
        axis.set(ylabel="Backlog [bytes]", title=f"{condition.label} (run 0)")
        axis.grid(alpha=0.3)
        axis.legend(fontsize="small")
    axes[-1].set_xlabel("Simulation time [s]")
    fig.suptitle("Reported and scheduled UL backlog")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "name": "heUlBufferStatusReportedBytes:vector"},
            {"type": "vector", "name": "heUlBufferStatusScheduledBytes:vector"},
        ],
        aggregation={"timeline": "representative run 0; event-driven step observations"},
    )


def plot_puncturing(conditions: list[Condition], output: Path) -> None:
    goodputs = [per_run_goodput(condition) for condition in conditions]
    dynamic = conditions[-1]
    offsets = representative_run(dynamic.vectors("heRuToneOffset:vector", module="**.ap.wlan[0].radio"))
    sizes = representative_run(dynamic.vectors("heRuToneSize:vector", module="**.ap.wlan[0].radio"))
    staids = representative_run(dynamic.vectors("heStaId:vector", module="**.ap.wlan[0].radio"))
    masks = representative_run(dynamic.vectors("hePuncturedSubchannelMask:vector", module="**.ap.wlan[0].radio"))
    if not (len(offsets) == len(sizes) == len(staids) == len(masks) == 1):
        raise RuntimeError("Runtime puncturing telemetry must resolve to one AP-radio vector per signal")
    offset_row, size_row, sta_row, mask_row = (frame.iloc[0] for frame in (offsets, sizes, staids, masks))
    if not np.array_equal(offset_row.vectime, size_row.vectime) or not np.array_equal(offset_row.vectime, sta_row.vectime):
        raise RuntimeError("Runtime puncturing RU telemetry timestamps are not aligned")
    observed_masks = set(np.asarray(mask_row.vecvalue, dtype=int))
    if not {0, 2}.issubset(observed_masks):
        raise RuntimeError(f"Runtime puncturing must observe masks 0 and 2, found {sorted(observed_masks)}")

    fig, axes = plt.subplots(1, 3, figsize=(18, 5))
    bar_with_ci(axes[0], [condition.label for condition in conditions], goodputs, "goodput_bps", scale=1e-6)
    axes[0].set_ylabel("Goodput [Mbit/s]")
    scatter = axes[1].scatter(
        offset_row.vectime,
        offset_row.vecvalue,
        s=np.maximum(12, np.asarray(size_row.vecvalue) / 2),
        c=sta_row.vecvalue,
        cmap="tab20",
        alpha=0.7,
    )
    axes[1].set(xlabel="Simulation time [s]", ylabel="RU tone offset", title="Runtime RU placement (run 0)")
    axes[1].grid(alpha=0.3)
    fig.colorbar(scatter, ax=axes[1], label="STA ID")
    axes[2].step(mask_row.vectime, np.asarray(mask_row.vecvalue, dtype=int), where="post")
    axes[2].set(xlabel="Simulation time [s]", ylabel="Punctured 20 MHz mask", title="Runtime mask 0 → 2 → 0")
    axes[2].grid(alpha=0.3)
    fig.suptitle("HE preamble puncturing under secondary-channel interference")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.app[*]", "name": "packetReceived:vector(packetBytes)", "unit": "B"},
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heRuToneOffset:vector"},
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heRuToneSize:vector"},
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heStaId:vector"},
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "hePuncturedSubchannelMask:vector"},
        ],
        aggregation={"goodput": "per run with 95% Student-t CI", "telemetry": "representative run 0"},
    )


def plot_mimo(conditions: list[Condition], output: Path) -> None:
    measured = conditions[-1]
    staids = representative_run(measured.vectors("heStaId:vector", module="**.ap.wlan[0].radio"))
    streams = representative_run(measured.vectors("heSpatialStreams:vector", module="**.ap.wlan[0].radio"))
    starts = representative_run(measured.vectors("heStreamStartIndex:vector", module="**.ap.wlan[0].radio"))
    if not (len(staids) == len(streams) == len(starts) == 1):
        raise RuntimeError("MU-MIMO telemetry must resolve to one AP-radio row per signal")
    sta_row, stream_row, start_row = staids.iloc[0], streams.iloc[0], starts.iloc[0]
    if not np.array_equal(sta_row.vectime, stream_row.vectime) or not np.array_equal(sta_row.vectime, start_row.vectime):
        raise RuntimeError("MU-MIMO telemetry timestamps are not aligned")
    times = np.asarray(sta_row.vectime)
    stations = np.asarray(sta_row.vecvalue, dtype=int)
    counts = np.asarray(stream_row.vecvalue, dtype=int)
    start_indices = np.asarray(start_row.vecvalue, dtype=int)
    for time in np.unique(times):
        selected = times == time
        validate_disjoint_streams(stations[selected], start_indices[selected], counts[selected])
    ppdu_times = np.unique(times)
    station_ids = np.unique(stations)
    matrix = np.full((len(station_ids), len(ppdu_times)), np.nan)
    station_index = {value: index for index, value in enumerate(station_ids)}
    time_index = {value: index for index, value in enumerate(ppdu_times)}
    for time, station, count in zip(times, stations, counts):
        matrix[station_index[station], time_index[time]] = count
    if not any(np.count_nonzero(~np.isnan(matrix[:, column])) >= 2 for column in range(matrix.shape[1])):
        raise RuntimeError("No PPDU serves multiple MU-MIMO users")

    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    image = axes[0].imshow(
        matrix,
        aspect="auto",
        interpolation="nearest",
        origin="lower",
        extent=(ppdu_times[0], ppdu_times[-1], -0.5, len(station_ids) - 0.5),
        vmin=0.5,
        cmap="viridis",
    )
    axes[0].set_yticks(range(len(station_ids)), station_ids)
    axes[0].set(xlabel="PPDU time [s]", ylabel="STA ID", title=f"{measured.label}: NSS per PPDU (run 0)")
    fig.colorbar(image, ax=axes[0], label="Allocated NSS")
    goodputs = [per_run_goodput(condition) for condition in conditions]
    bar_with_ci(axes[1], [condition.label for condition in conditions], goodputs, "goodput_bps", scale=1e-6)
    axes[1].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[1].set_title("Controlled 20 MHz comparison")
    fig.suptitle("MU-MIMO stream compatibility and delivery")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heStaId:vector"},
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heSpatialStreams:vector"},
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heStreamStartIndex:vector"},
            {"type": "vector", "module": "**.app[*]", "name": "packetReceived:vector(packetBytes)", "unit": "B"},
        ],
        aggregation={"telemetry": "all PPDUs validated; representative run 0 plotted", "goodput": "per run with 95% Student-t CI"},
    )


PLOTS: dict[str, Callable[[list[Condition], Path], None]] = {
    "fragmentation": plot_fragmentation,
    "uora": plot_uora,
    "twt": plot_twt,
    "rate": plot_rate,
    "puncturing": plot_puncturing,
    "mimo": plot_mimo,
    "bss": plot_bss,
    "width": plot_width,
    "dl": plot_dl,
    "bsr": plot_bsr,
}
