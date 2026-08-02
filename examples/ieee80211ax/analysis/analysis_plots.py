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
    goodputs: list[pd.DataFrame] = []
    delays: list[pd.DataFrame] = []
    attempts_per_run: list[pd.DataFrame] = []
    successful_transmissions: list[pd.DataFrame] = []
    zero_success_counts: dict[str, int] = {}
    for condition in conditions:
        goodputs.append(per_run_goodput(condition))
        delays.append(per_run_delay_percentile(condition, 95))
        attempts = condition.scalars("heUlRandomAccessAttempt:count")
        success = condition.scalars("heUlRandomAccessSuccess:count")
        attempts = attempts[attempts.module.str.contains(".host[", regex=False)]
        success = success[success.module.str.contains(".host[", regex=False)]
        attempt_totals = attempts.groupby("runID", as_index=False).value.sum().rename(columns={"value": "attempts"})
        success_totals = success.groupby("runID", as_index=False).value.sum().rename(columns={"value": "successes"})
        merged = attempt_totals.merge(success_totals, on="runID", validate="one_to_one")
        attempts_per_run.append(merged[["runID", "attempts"]])
        successful_transmissions.append(merged[["runID", "successes"]])
        zero_success_counts[condition.config] = int(
            (merged.successes == 0).sum()
        )
    labels = [condition.label for condition in conditions]
    fig, axes = plt.subplots(2, 2, figsize=(15, 9.2))
    bar_with_ci(axes[0, 0], labels, goodputs, "goodput_bps", scale=1e-6)
    bar_with_ci(axes[0, 1], labels, delays, "delay_s", scale=1e3)
    bar_with_ci(axes[1, 0], labels, attempts_per_run, "attempts")
    bar_with_ci(
        axes[1, 1],
        labels,
        successful_transmissions,
        "successes",
    )
    axes[0, 0].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[0, 1].set_ylabel("95th-percentile end-to-end delay [ms]")
    axes[1, 0].set_ylabel("UORA attempts per run")
    axes[1, 1].set_ylabel("Successful UORA transmissions per run")
    fig.suptitle("Scheduled uplink and UORA outcome comparison")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.server.app[*]", "name": "packetReceived:vector(packetBytes)", "value_semantics": "bytes from packetBytes recorder; unit attribute empty"},
            {"type": "vector", "module": "**.server.app[*]", "name": "endToEndDelay:vector", "unit": "s"},
            {"type": "scalar", "name": "heUlRandomAccessAttempt:count"},
            {"type": "scalar", "name": "heUlRandomAccessSuccess:count"},
        ],
        aggregation={
            "goodput": "sum delivered application bytes over the manifest measurement window across sink vectors, convert to bit/s; one value per run",
            "delay": "pool delivered-packet delays within each run over the manifest measurement window, then take the 95th percentile; one value per run",
            "mechanism": "terminal full-simulation scalar counters summed across stations; one value per run",
            "zero_success_runs": zero_success_counts,
            "uncertainty": "95% Student-t CI",
        },
        extra={"result_session_id": output.parent.name},
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

    fig, axes = plt.subplots(1, 2, figsize=(15, 5))
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


def _per_run_event_count(condition: Condition, name: str, module: str, required: bool = True) -> pd.DataFrame:
    frame = condition.vectors(name, module=module, required=required)
    records = []
    run_ids = [pair.run_number for pair in condition.result_files]
    for run_number in run_ids:
        rows = frame[pd.to_numeric(frame.runnumber, errors="raise").astype(int) == run_number] if not frame.empty else frame
        count = sum(
            len(crop_vector(row.vectime, row.vecvalue, condition.measurement)[1])
            for _, row in rows.iterrows()
        )
        records.append({"runID": run_number, "count": count})
    return pd.DataFrame.from_records(records)


def plot_er(conditions: list[Condition], output: Path) -> None:
    if len(conditions) != 2:
        raise RuntimeError("HE ER SU boundary comparison requires HE-SU and HE-ER-SU conditions")
    delivered = [
        _per_run_event_count(condition, "packetReceived:vector(packetBytes)", "**.app[*]")
        for condition in conditions
    ]
    drops = [
        _per_run_event_count(condition, "packetDropIncorrectlyReceived:vector(packetBytes)", "**.mac", required=False)
        for condition in conditions
    ]
    goodputs = [per_run_goodput(condition) for condition in conditions]
    if any(np.any(frame["count"] <= 0) for frame in delivered):
        raise RuntimeError("HE ER SU boundary campaign did not preserve application delivery")

    labels = [condition.label for condition in conditions]
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.8))
    bar_with_ci(axes[0], labels, delivered, "count")
    bar_with_ci(axes[1], labels, drops, "count")
    bar_with_ci(axes[2], labels, goodputs, "goodput_bps", scale=1e-6)
    axes[0].set_ylabel("Delivered application packets")
    axes[1].set_ylabel("Incorrectly received MAC observations")
    axes[2].set_ylabel("Application goodput [Mbit/s]")
    fig.suptitle("HE-SU versus HE-ER-SU at the controlled cell boundary")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.app[*]", "name": "packetReceived:vector(packetBytes)", "unit": "B"},
            {"type": "vector", "module": "**.mac", "name": "packetDropIncorrectlyReceived:vector(packetBytes)", "unit": "B", "optional_when_zero": True},
        ],
        aggregation={
            "observation": "per-run manifest measurement window",
            "uncertainty": "95% Student-t CI over five seeds",
            "interpretation": "diagnostic delivery comparison; the standard does not guarantee a gain for this channel/error model",
        },
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


def _measurement_vector_values(condition: Condition, name: str) -> np.ndarray:
    frame = condition.vectors(name, module="**.receiver")
    values = [
        crop_vector(row.vectime, row.vecvalue, condition.measurement)[1]
        for _, row in frame.iterrows()
    ]
    return np.concatenate(values) if values else np.array([], dtype=float)


SPATIAL_REUSE_DECISION_VECTORS = {
    "bss_type": "heSpatialReuseBssType:vector",
    "received_bss_color": "heSpatialReuseReceivedBssColor:vector",
    "local_bss_color": "heSpatialReuseLocalBssColor:vector",
    "received_power": "heSpatialReuseReceivedPower:vector",
    "eligible": "heSpatialReuseEligible:vector",
    "ignored_ppdu": "heSpatialReuseIgnoredPpdu:vector",
    "threshold": "heSpatialReuseObssPdThreshold:vector",
    "power_limit": "heSpatialReuseTransmitPowerLimit:vector",
    "reason": "heSpatialReuseReason:vector",
}


def _spatial_reuse_decisions(condition: Condition) -> pd.DataFrame:
    """Join receiver-emitted spatial-reuse decision vectors by their common sample."""
    indexed_vectors: dict[str, pd.DataFrame] = {}
    expected_keys: set[tuple[str, str]] | None = None
    for column, name in SPATIAL_REUSE_DECISION_VECTORS.items():
        frame = condition.vectors(name, module="**.receiver")
        required_columns = {"runID", "module", "vectime", "vecvalue"}
        missing_columns = required_columns - set(frame.columns)
        if missing_columns:
            raise RuntimeError(
                f"{condition.config}/{name}: missing {sorted(missing_columns)}"
            )
        try:
            indexed = frame.set_index(["runID", "module"], verify_integrity=True)
        except ValueError as error:
            raise RuntimeError(
                f"{condition.config}/{name}: duplicate receiver vector rows"
            ) from error
        keys = set(indexed.index.tolist())
        if expected_keys is None:
            expected_keys = keys
        elif keys != expected_keys:
            raise RuntimeError(
                f"{condition.config}: spatial-reuse vectors have different receiver rows"
            )
        indexed_vectors[column] = indexed

    records: list[dict[str, Any]] = []
    for run_id, module in sorted(expected_keys or set()):
        reference_times = np.asarray(
            indexed_vectors["reason"].loc[(run_id, module), "vectime"], dtype=float
        )
        values_by_column: dict[str, np.ndarray] = {}
        for column, frame in indexed_vectors.items():
            row = frame.loc[(run_id, module)]
            times = np.asarray(row.vectime, dtype=float)
            values = np.asarray(row.vecvalue, dtype=float)
            if not np.array_equal(times, reference_times):
                raise RuntimeError(
                    f"{condition.config}/{module}/{run_id}: unaligned "
                    f"spatial-reuse timestamps for {SPATIAL_REUSE_DECISION_VECTORS[column]}"
                )
            if len(values) != len(reference_times):
                raise RuntimeError(
                    f"{condition.config}/{module}/{run_id}: unaligned "
                    f"spatial-reuse values for {SPATIAL_REUSE_DECISION_VECTORS[column]}"
                )
            values_by_column[column] = values
        sample_indices = np.flatnonzero(
            (reference_times >= condition.measurement.start) &
            (reference_times < condition.measurement.end)
        )
        for sample_index in sample_indices:
            records.append({
                "runID": run_id,
                "module": module,
                "sample_index": int(sample_index),
                "time": reference_times[sample_index],
                **{
                    column: values[sample_index]
                    for column, values in values_by_column.items()
                },
            })
    return pd.DataFrame.from_records(records)


def _validate_bss_spatial_reuse(condition: Condition) -> None:
    expectation = condition.condition_metadata["spatial_reuse_evidence"]
    decisions = _spatial_reuse_decisions(condition)
    if decisions.empty:
        raise RuntimeError(f"{condition.config}: missing HE spatial-reuse decision telemetry")
    reasons = decisions.reason.to_numpy(dtype=int)
    eligible = decisions.eligible.to_numpy(dtype=int)
    bss_types = decisions.bss_type.to_numpy(dtype=int)

    if expectation == "disabled":
        if np.any(eligible != 0):
            raise RuntimeError(f"{condition.config}: disabled spatial reuse produced eligible PPDUs")
        return

    if expectation == "collision":
        if np.any(eligible != 0) or np.any(np.isin(reasons, [11, 12])):
            raise RuntimeError(f"{condition.config}: same-color PPDUs were treated as inter-BSS OBSS/PD candidates")
        if not np.any(reasons == 4):
            raise RuntimeError(f"{condition.config}: no same-color intra-BSS decision was observed")
        return

    # Reason 11/12 means an eligible inter-BSS PPDU was respectively below or
    # at/above OBSS/PD. BSS type 2/3 is non-SRG/SRG inter-BSS.
    candidate_decisions = decisions[decisions.reason.isin([11, 12])]
    if candidate_decisions.empty:
        raise RuntimeError(f"{condition.config}: no eligible inter-BSS OBSS/PD decision was observed")
    if (
        np.any(~candidate_decisions.bss_type.isin([2, 3])) or
        np.any(candidate_decisions.eligible != 1) or
        np.any(candidate_decisions.ignored_ppdu != (candidate_decisions.reason == 11))
    ):
        raise RuntimeError(
            f"{condition.config}: OBSS/PD decision does not match its classification or outcome"
        )

    thresholds = candidate_decisions.threshold.to_numpy(dtype=float)
    power_limits = candidate_decisions.power_limit.to_numpy(dtype=float)
    received_powers = candidate_decisions.received_power.to_numpy(dtype=float)
    if (not np.all(np.isfinite(received_powers)) or
            not np.all(np.isfinite(thresholds)) or
            not np.all(np.isfinite(power_limits))):
        raise RuntimeError(f"{condition.config}: received-power, OBSS/PD threshold, or coupled TX-power telemetry is missing")
    expected_threshold = float(condition.condition_metadata["obss_pd_dbm"])
    expected_power_limit = 21.0 - max(0.0, expected_threshold - (-82.0))
    # The native result API normalizes logarithmic dBm samples to linear mW
    # while retaining the source unit metadata.
    expected_threshold_mw = 10 ** (expected_threshold / 10.0)
    expected_power_limit_mw = 10 ** (expected_power_limit / 10.0)
    if not np.allclose(thresholds, expected_threshold_mw, rtol=1e-9, atol=0):
        raise RuntimeError(f"{condition.config}: recorded OBSS/PD threshold differs from configuration")
    if not np.allclose(power_limits, expected_power_limit_mw, rtol=1e-9, atol=0):
        raise RuntimeError(
            f"{condition.config}: TX-power limit violates the 21 dBm/-82 dBm OBSS/PD coupling"
        )


def plot_bss(conditions: list[Condition], output: Path) -> None:
    for condition in conditions:
        _validate_bss_spatial_reuse(condition)
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
            {"type": "vector", "module": "**.receiver", "name": "heSpatialReuseReason:vector", "reason_codes": {"11": "below OBSS/PD", "12": "at/above OBSS/PD"}},
            {"type": "vector", "module": "**.receiver", "name": "heSpatialReuseBssType:vector", "inter_bss_codes": [2, 3]},
            {"type": "vector", "module": "**.receiver", "name": "heSpatialReuseReceivedPower:vector", "unit": "dBm"},
            {"type": "vector", "module": "**.receiver", "name": "heSpatialReuseEligible:vector"},
            {"type": "vector", "module": "**.receiver", "name": "heSpatialReuseObssPdThreshold:vector", "unit": "dBm"},
            {"type": "vector", "module": "**.receiver", "name": "heSpatialReuseTransmitPowerLimit:vector", "unit": "dBm"},
        ],
        aggregation={
            "observation": "per-run measurement-window aggregate",
            "uncertainty": "95% Student-t CI",
            "validation": "joins each receiver decision by run, module, and aligned vector sample; requires inter-BSS OBSS/PD decisions and validates the 21 dBm/-82 dBm threshold-to-power relation",
        },
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
    for condition in conditions:
        if condition.condition_metadata.get("workload") and condition.label != "EDCA":
            staids = condition.vectors("heStaId:vector", module="**.ap.wlan[0].radio")
            # The native result API normalizes byte-valued samples to its base
            # bit unit even though the vector file and signal are declared B.
            scheduled = condition.vectors("heScheduledPsduBytes:vector", module="**.ap.wlan[0].radio", expected_unit="b")
            durations = condition.vectors("heUserPpduDuration:vector", module="**.ap.wlan[0].radio", expected_unit="s")
            for run_id in sorted(set(staids.runID)):
                sta_rows = staids[staids.runID == run_id]
                byte_rows = scheduled[scheduled.runID == run_id]
                duration_rows = durations[durations.runID == run_id]
                if not (len(sta_rows) == len(byte_rows) == len(duration_rows) == 1):
                    raise RuntimeError(f"{condition.config}/{run_id}: per-user scheduling vectors must have one AP-radio row")
                sta_row, byte_row, duration_row = sta_rows.iloc[0], byte_rows.iloc[0], duration_rows.iloc[0]
                if not (np.array_equal(sta_row.vectime, byte_row.vectime) and
                        np.array_equal(sta_row.vectime, duration_row.vectime)):
                    raise RuntimeError(f"{condition.config}/{run_id}: per-user scheduling telemetry is not aligned")
                if np.any(np.asarray(byte_row.vecvalue) < 0) or np.any(np.asarray(duration_row.vecvalue) <= 0):
                    raise RuntimeError(f"{condition.config}/{run_id}: invalid scheduled bytes or airtime")

    workloads = [w for w in ["symmetric", "asymmetric"] if any(c.condition_metadata.get("workload") == w for c in conditions)]
    if not workloads:
        workloads = ["asymmetric"]
    fig, axes = plt.subplots(len(workloads), 3, figsize=(15, 4.8 * len(workloads)))
    axes = np.atleast_2d(axes)
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
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heStaId:vector"},
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heScheduledPsduBytes:vector", "unit": "B"},
            {"type": "vector", "module": "**.ap.wlan[0].radio", "name": "heUserPpduDuration:vector", "unit": "s"},
        ],
        aggregation={"observation": "one per run", "per_user": "STA-ID aligned scheduled PSDU bytes and PPDU duration", "delay": "pooled packet p95 within run", "uncertainty": "95% Student-t CI"},
    )


def plot_bsr(conditions: list[Condition], output: Path) -> None:
    visible_aids = {1, 2}
    fig, axes = plt.subplots(len(conditions), 1, figsize=(12, 4 * len(conditions)), sharex=True)
    axes = np.atleast_1d(axes)
    for axis, condition in zip(axes, conditions):
        reported = representative_run(condition.vectors("heUlBufferStatusReportedBytes:vector"))
        reported_aids = representative_run(condition.vectors("heUlBufferStatusUpdated:vector"))
        scheduled_vectors = condition.vectors(
            "heUlBufferStatusScheduledBytes:vector",
            required=False,
            allow_missing_runs=True,
        )
        scheduled = representative_run(scheduled_vectors) if not scheduled_vectors.empty else scheduled_vectors
        scheduled_aids = representative_run(
            condition.vectors(
                "heUlTriggerDecisionAssociationId:vector",
                required=False,
                allow_missing_runs=True,
            )
        )
        colors = plt.get_cmap("tab10")

        if len(reported) != len(reported_aids):
            raise RuntimeError("Reported BSR byte and AID vectors are not aligned")
        for (_, value_row), (_, aid_row) in zip(reported.iterrows(), reported_aids.iterrows()):
            if len(value_row.vectime) != len(aid_row.vectime) or not np.array_equal(value_row.vectime, aid_row.vectime):
                raise RuntimeError("Reported BSR byte and AID vector timestamps are not aligned")
            for aid in sorted({int(value) for value in aid_row.vecvalue} & visible_aids):
                mask = np.asarray(aid_row.vecvalue, dtype=int) == aid
                times, values = crop_vector(value_row.vectime[mask], value_row.vecvalue[mask], condition.measurement)
                if len(times):
                    color = colors((aid - 1) % 10 / 9)
                    axis.step(times, values, where="post", color=color, linestyle="-", alpha=0.85, label=f"STA {aid} reported")

        if not scheduled.empty:
            if len(scheduled) != len(scheduled_aids):
                raise RuntimeError("Scheduled BSR byte and association vectors are not aligned by run")
            for (_, value_row), (_, aid_row) in zip(scheduled.iterrows(), scheduled_aids.iterrows()):
                association_times = np.asarray(aid_row.vectime)
                association_values = np.asarray(aid_row.vecvalue, dtype=int)
                used = np.zeros(len(association_times), dtype=bool)
                scheduled_aids_for_values = []
                for time in np.asarray(value_row.vectime):
                    matches = np.flatnonzero(
                        ~used & np.isclose(association_times, time, rtol=0, atol=1e-12)
                    )
                    if len(matches) == 0:
                        raise RuntimeError("Scheduled BSR timestamps do not match trigger-decision association timestamps")
                    match = matches[0]
                    used[match] = True
                    scheduled_aids_for_values.append(association_values[match])
                scheduled_aids_for_values = np.asarray(scheduled_aids_for_values, dtype=int)
                for aid in sorted(set(scheduled_aids_for_values) & visible_aids):
                    mask = scheduled_aids_for_values == aid
                    times, values = crop_vector(np.asarray(value_row.vectime)[mask], np.asarray(value_row.vecvalue)[mask], condition.measurement)
                    if len(times):
                        color = colors((aid - 1) % 10 / 9)
                        axis.step(times, values, where="post", color=color, linestyle="--", alpha=0.9, label=f"STA {aid} scheduled")
        if scheduled.empty:
            axis.text(
                0.5, 0.5, "No scheduled bytes observed in run 0",
                transform=axis.transAxes, ha="center", va="center",
            )
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
            {"type": "vector", "name": "heUlBufferStatusUpdated:vector", "unit": "AID"},
            {"type": "vector", "name": "heUlBufferStatusReportedBytes:vector"},
            {"type": "vector", "name": "heUlBufferStatusScheduledBytes:vector"},
            {"type": "vector", "name": "heUlTriggerDecisionAssociationId:vector", "unit": "AID"},
        ],
        aggregation={"timeline": "representative run 0; event-driven step observations", "grouping": "STA association ID; solid=reported, dashed=scheduled"},
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

    fig, axes = plt.subplots(1, 2, figsize=(18, 5.5), gridspec_kw={"width_ratios": [1, 1.35]})
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
    plot_labels = []
    for condition in conditions:
        load = condition.condition_metadata.get("offered_aggregate_mbps")
        method = "OFDMA" if "OFDMA" in condition.label else "MU-MIMO"
        label = f"{method}\n{load:g}" if load is not None else method
        if condition.condition_metadata.get("csi_leakage") == 0.001:
            label += "\nleak .001"
        if condition.condition_metadata.get("sta_antenna_count") == 1:
            label += "\nSTA 1 ant"
        plot_labels.append(label)
    bar_with_ci(axes[1], plot_labels, goodputs, "goodput_bps", scale=1e-6)
    axes[1].tick_params(axis="x", rotation=28, labelsize=8)
    for label in axes[1].get_xticklabels():
        label.set_horizontalalignment("right")
    axes[1].set_xlabel("Method and aggregate offered load [Mbit/s]")
    axes[1].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[1].set_title("20 MHz offered-load and STA-antenna comparisons")
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


def plot_delivery(conditions: list[Condition], output: Path) -> None:
    """Plot the shared five-run application-delivery baseline."""
    goodputs = [per_run_goodput(condition) for condition in conditions]
    fig, axis = plt.subplots(figsize=(max(8, len(conditions) * 1.4), 4.8))
    bar_with_ci(
        axis,
        [condition.label for condition in conditions],
        goodputs,
        "goodput_bps",
        scale=1e-6,
    )
    axis.set_ylabel("Aggregate goodput [Mbit/s]")
    axis.set_title("Five-run application delivery")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[{
            "type": "vector",
            "module": "**.app[*]",
            "name": "packetReceived:vector(packetBytes)",
            "value_semantics": "delivered application bytes",
        }],
        aggregation={
            "observation": "one aggregate goodput value per run",
            "uncertainty": "95% Student-t CI",
        },
    )


def plot_ul_ofdma(conditions: list[Condition], output: Path) -> None:
    """Compare UL-OFDMA and EDCA using delivery and tail delay."""
    ul_ofdma_order = {
        "EDCA baseline (5 ms)": 0,
        "Equal-sized RUs (5 ms)": 1,
        "Backlog scheduler (5 ms)": 2,
        "EDCA baseline (2.5 ms)": 3,
        "Equal-sized RUs (2.5 ms)": 4,
        "Backlog scheduler (2.5 ms)": 5,
        "EDCA baseline (1 ms)": 6,
        "Equal-sized RUs (1 ms)": 7,
        "Backlog scheduler (1 ms)": 8,
        "Asymmetric backlog": 9,
        "EDCA baseline": 0,
        "Equal-sized RUs": 1,
        "Backlog scheduler": 2,
    }
    conditions = sorted(
        conditions,
        key=lambda condition: (
            ul_ofdma_order.get(condition.label, len(ul_ofdma_order)),
            condition.label,
        ),
    )
    bidirectional = conditions and conditions[0].group == "dl_ul_ofdma"
    goodputs = [per_run_goodput(condition) for condition in conditions]
    delays = [per_run_delay_percentile(condition, 95) for condition in conditions]
    labels = [condition.label for condition in conditions]
    fig, axes = plt.subplots(1, 2, figsize=(13, 4.8))
    bar_with_ci(axes[0], labels, goodputs, "goodput_bps", scale=1e-6)
    bar_with_ci(axes[1], labels, delays, "delay_s", scale=1e3)
    axes[0].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[1].set_ylabel("95th-percentile end-to-end delay [ms]")
    axes[0].set_title("Application delivery")
    axes[1].set_title("Tail delay")
    for axis in axes:
        axis.grid(axis="y", alpha=0.3)
    fig.suptitle(
        "Bidirectional OFDMA and SU delivery comparison"
        if bidirectional else "UL-OFDMA contention and delivery comparison"
    )
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {"type": "vector", "module": "**.app[*]", "name": "packetReceived:vector(packetBytes)", "value_semantics": "delivered application bytes"},
            {"type": "vector", "module": "**.app[*]", "name": "endToEndDelay:vector", "unit": "s"},
        ],
        aggregation={
            "goodput": "sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run",
            "delay": "pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run",
            "uncertainty": "95% Student-t CI across independent runs",
        },
    )

def plot_dl_bar(conditions: list[Condition], output: Path) -> None:
    """Compare DL-BAR acknowledgment methods using aggregate goodput and 95th-percentile delay."""
    goodputs = [per_run_goodput(condition) for condition in conditions]
    delays = [per_run_delay_percentile(condition, 95) for condition in conditions]
    labels = [condition.label for condition in conditions]
    fig, axes = plt.subplots(1, 2, figsize=(max(12, len(conditions) * 2.2), 5.0))
    bar_with_ci(axes[0], labels, goodputs, "goodput_bps", scale=1e-6)
    bar_with_ci(axes[1], labels, delays, "delay_s", scale=1e3)
    axes[0].set_ylabel("Aggregate goodput [Mbit/s]")
    axes[1].set_ylabel("95th-percentile end-to-end delay [ms]")
    axes[0].set_title("Application goodput")
    axes[1].set_title("95th-percentile delay")
    for axis in axes:
        axis.grid(axis="y", alpha=0.3)
    fig.suptitle("Downlink OFDMA Block Ack Request comparison")
    save(fig, output)
    write_provenance(
        output,
        conditions=conditions,
        result_filters=[
            {
                "type": "vector",
                "module": "**.app[*]",
                "name": "packetReceived:vector(packetBytes)",
                "value_semantics": "delivered application bytes",
            },
            {
                "type": "vector",
                "module": "**.app[*]",
                "name": "endToEndDelay:vector",
                "value_semantics": "end-to-end packet delivery delay",
            },
        ],
        aggregation={
            "observation": "one aggregate goodput and 95th-percentile delay per run",
            "uncertainty": "95% Student-t CI",
        },
    )

PLOTS: dict[str, Callable[[list[Condition], Path], None]] = {
    "fragmentation": plot_fragmentation,
    "uora": plot_uora,
    "twt": plot_twt,
    "rate": plot_rate,
    "er": plot_er,
    "puncturing": plot_puncturing,
    "mimo": plot_mimo,
    "bss": plot_bss,
    "width": plot_width,
    "dl_sched": plot_dl,
    "dl_asym": plot_dl,
    "dl_bar": plot_dl_bar,
    "bsr": plot_bsr,
    "multi_tid": plot_delivery,
    "operating_mode": plot_delivery,
    "frequency_selective": plot_delivery,
    "ndp_feedback": plot_delivery,
    "dense_iot": plot_delivery,
    "eht_features": plot_delivery,
    "bcc_ldpc": plot_delivery,
    "ul_mu_mimo": plot_delivery,
    "ul_ofdma": plot_ul_ofdma,
    "dl_ul_ofdma": plot_ul_ofdma,
}
