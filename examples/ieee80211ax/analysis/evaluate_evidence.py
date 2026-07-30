#!/usr/bin/env python3
"""Evaluate executable scalar/vector evidence contracts into a durable ledger."""

from __future__ import annotations

import argparse
import json
import math
from collections import Counter
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from decimal import Decimal
from pathlib import Path
from typing import Any, Iterable

import numpy as np
from omnetpp.scave import results as scave_results

from analysis_core import (
    DEFAULT_MANIFEST,
    QUERY_OPTIONS,
    REPOSITORY_ROOT,
    SESSION_ID_PATTERN,
    Condition,
    app_sink_vectors,
    atomic_write_text,
    conditions_for_group,
    crop_vector,
    git_revision,
    load_manifest,
    resolve_session_id,
)

STATUSES = {"PASS", "FAIL", "INCONCLUSIVE", "NOT RUN"}
DEFAULT_OUTPUT = Path(__file__).resolve().parent / "evidence-ledger.json"


@dataclass(frozen=True)
class Evaluation:
    status: str
    reason: str
    observations: list[dict[str, Any]]

    def __post_init__(self) -> None:
        if self.status not in STATUSES:
            raise ValueError(f"Invalid evidence status {self.status!r}")


class MissingDiagnosticTelemetryError(RuntimeError):
    def __init__(self, config: str, vector: str, diagnostic_run: int) -> None:
        super().__init__(
            f"{config}/{vector}: diagnostic telemetry is unavailable for "
            f"diagnostic run {diagnostic_run}; rerun the campaign with "
            "--exhaustive-vectors"
        )


def evaluate_mimo_triplets(
    per_run: dict[int, list[tuple[float, int, int, int]]]
) -> Evaluation:
    """Check that every observed multi-user PPDU has disjoint stream ranges."""
    observations: list[dict[str, Any]] = []
    failures: list[str] = []
    total_multi_user = 0
    for run_number, samples in sorted(per_run.items()):
        by_time: dict[float, list[tuple[int, int, int]]] = {}
        for timestamp, station, count, start in samples:
            by_time.setdefault(float(timestamp), []).append(
                (int(station), int(start), int(count))
            )
        run_multi_user = 0
        run_overlap = 0
        for timestamp, allocations in sorted(by_time.items()):
            if any(count <= 0 or start < 0 for _, start, count in allocations):
                failures.append(f"run {run_number} at {timestamp}: invalid stream range")
                continue
            stations = {station for station, _, _ in allocations}
            if len(stations) < 2:
                continue
            run_multi_user += 1
            intervals = sorted(
                (start, start + count, station)
                for station, start, count in allocations
            )
            if any(left[1] > right[0] for left, right in zip(intervals, intervals[1:])):
                run_overlap += 1
                failures.append(f"run {run_number} at {timestamp}: overlapping streams")
        total_multi_user += run_multi_user
        observations.append({
            "run_number": run_number,
            "sample_count": len(samples),
            "ppdu_timestamp_count": len(by_time),
            "multi_user_ppdu_count": run_multi_user,
            "overlap_count": run_overlap,
        })
    if failures:
        return Evaluation("FAIL", "; ".join(failures[:8]), observations)
    if not per_run or total_multi_user == 0:
        return Evaluation(
            "FAIL",
            "No timestamp contained allocations for multiple distinct station IDs.",
            observations,
        )
    missing_runs = [
        item["run_number"] for item in observations
        if item["multi_user_ppdu_count"] == 0
    ]
    if missing_runs:
        return Evaluation(
            "FAIL",
            f"No multi-user PPDU was observed in runs {missing_runs}.",
            observations,
        )
    return Evaluation(
        "PASS",
        "Every run contains a multi-user PPDU and all same-timestamp stream ranges are disjoint.",
        observations,
    )


def evaluate_matched_delivery(
    baseline: dict[tuple[int, int], float],
    treatment: dict[tuple[int, int], float],
    minimum_ratio: float,
) -> Evaluation:
    """Compare delivery for runs paired by both run number and recorded seed set."""
    baseline_keys = set(baseline)
    treatment_keys = set(treatment)
    if baseline_keys != treatment_keys:
        return Evaluation(
            "INCONCLUSIVE",
            "Baseline and treatment do not have identical (run number, seed set) pairs.",
            [{
                "baseline_pairs": [list(key) for key in sorted(baseline_keys)],
                "treatment_pairs": [list(key) for key in sorted(treatment_keys)],
            }],
        )
    observations = []
    for run_number, seed_set in sorted(baseline_keys):
        baseline_bytes = float(baseline[(run_number, seed_set)])
        treatment_bytes = float(treatment[(run_number, seed_set)])
        if not all(map(math.isfinite, (baseline_bytes, treatment_bytes))):
            raise ValueError("Delivery observations must be finite")
        if baseline_bytes <= 0:
            return Evaluation(
                "INCONCLUSIVE",
                f"Baseline delivery is nonpositive for run {run_number}, seed set {seed_set}.",
                observations,
            )
        ratio = treatment_bytes / baseline_bytes
        observations.append({
            "run_number": run_number,
            "seed_set": seed_set,
            "baseline_delivered_bytes": baseline_bytes,
            "treatment_delivered_bytes": treatment_bytes,
            "delivery_ratio": ratio,
            "minimum_ratio": minimum_ratio,
        })
    if not observations:
        return Evaluation("INCONCLUSIVE", "No matched delivery observations exist.", [])
    failed = [
        item for item in observations if item["delivery_ratio"] < minimum_ratio
    ]
    if failed:
        pairs = [(item["run_number"], item["seed_set"]) for item in failed]
        return Evaluation(
            "FAIL",
            f"Delivery ratio is below {minimum_ratio} for run/seed pairs {pairs}.",
            observations,
        )
    return Evaluation(
        "PASS",
        f"Every matched run preserves at least {minimum_ratio:.3f} of baseline delivery.",
        observations,
    )


def _he_ru_offsets(root_tone_size: int, target_tone_size: int) -> list[int]:
    offsets = []

    def visit(tone_size: int, tone_offset: int) -> None:
        if tone_size == target_tone_size:
            offsets.append(tone_offset)
        children = {
            1992: ((996, 0), (996, 996)),
            996: ((484, 0), (26, 485), (484, 512)),
            484: ((242, 0), (242, 242)),
            242: ((106, 0), (26, 108), (106, 136)),
            106: ((52, 0), (52, 54)),
            52: ((26, 0), (26, 26)),
            26: (),
        }[tone_size]
        for child_size, child_offset in children:
            visit(child_size, tone_offset + child_offset)

    visit(root_tone_size, 0)
    return sorted(offsets)


def decode_he_trigger_ru(
    ul_bandwidth: int, allocation: int, allocation_region: int = 0,
) -> tuple[int, int]:
    """Decode the HE Trigger RU Allocation field into INET RU geometry."""
    # IEEE Std 802.11-2024, Clause 9.3.1.22 and Tables 9-53 and
    # 27-8 through 27-10: UL BW, RU Allocation B0, and B7-B1 jointly
    # identify the Trigger User Info RU size and location.
    if ul_bandwidth not in {0, 1, 2, 3}:
        raise ValueError(f"Invalid HE Trigger UL bandwidth {ul_bandwidth}")
    if allocation < 0 or allocation > 127:
        raise ValueError(f"Invalid HE Trigger RU Allocation {allocation}")
    if allocation_region not in {0, 1}:
        raise ValueError(
            f"Invalid HE Trigger RU Allocation Region {allocation_region}"
        )
    half = allocation_region
    code = allocation
    if ul_bandwidth != 3 and half:
        raise ValueError(
            f"HE Trigger RU Allocation {allocation} selects a 160 MHz half "
            f"for UL bandwidth {ul_bandwidth}"
        )
    if code == 68:
        if ul_bandwidth != 3 or half:
            raise ValueError(f"Invalid 2x996-tone HE Trigger allocation {allocation}")
        return 1992, 0
    ranges = (
        (0, 36, 26),
        (37, 52, 52),
        (53, 60, 106),
        (61, 64, 242),
        (65, 66, 484),
        (67, 67, 996),
    )
    for first, last, tone_size in ranges:
        if first <= code <= last:
            index = code - first
            root_tone_size = {0: 242, 1: 484, 2: 996, 3: 996}[ul_bandwidth]
            offsets = _he_ru_offsets(root_tone_size, tone_size)
            if index >= len(offsets):
                raise ValueError(
                    f"HE Trigger RU Allocation {allocation} is outside the "
                    f"{root_tone_size}-tone catalog"
                )
            return tone_size, offsets[index] + (996 if ul_bandwidth == 3 and half else 0)
    raise ValueError(f"Reserved HE Trigger RU Allocation {allocation}")


def _timestamp_key(value: Any) -> Decimal:
    return Decimal(str(value))


def evaluate_ul_trigger_allocation_join(
    model_rows: dict[str, list[dict[str, Any]]],
    packet_rows: dict[str, list[dict[str, Any]]],
) -> Evaluation:
    """Join each committed Basic Trigger user to decoded PCAP User Info."""
    maximum_commit_to_capture_delay = Decimal("0.001")
    observations = []
    failures = []
    inconclusive = []
    for config in sorted(set(model_rows) | set(packet_rows)):
        models = model_rows.get(config, [])
        packets = [
            row for row in packet_rows.get(config, [])
            if row.get("trigger_type") == 0
        ]
        models_by_time: dict[Decimal, list[dict[str, Any]]] = {}
        for row in models:
            if int(row["trigger_type"]) != 0:
                continue
            models_by_time.setdefault(_timestamp_key(row["simulation_time"]), []).append(row)
        packets_by_time: dict[Decimal, list[dict[str, Any]]] = {}
        for row in packets:
            packets_by_time.setdefault(
                _timestamp_key(row["simulation_time"]), []
            ).append(row)
        if not models_by_time and not packets_by_time:
            inconclusive.append(f"{config}: no committed or decoded Basic Triggers")
            continue
        model_decisions = sorted(models_by_time.items())
        packet_triggers = sorted(
            [
                (timestamp, packet_trigger)
                for timestamp, triggers in packets_by_time.items()
                for packet_trigger in triggers
            ],
            key=lambda item: (item[0], int(item[1]["frame_number"])),
        )
        if len(model_decisions) != len(packet_triggers):
            inconclusive.append(
                f"{config}: found {len(model_decisions)} committed Basic "
                f"Triggers and {len(packet_triggers)} decoded PCAP Triggers"
            )
            continue
        for (commit_time, model_users), (
            capture_time,
            packet_trigger,
        ) in zip(model_decisions, packet_triggers):
            trigger_ids = {int(row["trigger_id"]) for row in model_users}
            if len(trigger_ids) != 1:
                inconclusive.append(
                    f"{config} at {commit_time}: expected one model decision, "
                    f"found trigger IDs {sorted(trigger_ids)}"
                )
                continue
            capture_delay = capture_time - commit_time
            if (
                capture_delay < 0
                or capture_delay > maximum_commit_to_capture_delay
            ):
                inconclusive.append(
                    f"{config} decision at {commit_time}: ordered PCAP Trigger "
                    f"at {capture_time} has non-causal or excessive delay "
                    f"{capture_delay} s"
                )
                continue
            if not packet_trigger.get("field_cardinality_consistent", False):
                inconclusive.append(
                    f"{config} at {commit_time}: inconsistent repeated Trigger "
                    "User Info field cardinalities"
                )
                continue
            ordered_models = sorted(
                model_users, key=lambda row: int(row["user_ordinal"])
            )
            ordinals = [int(row["user_ordinal"]) for row in ordered_models]
            if ordinals != list(range(len(ordered_models))):
                inconclusive.append(
                    f"{config} at {commit_time}: model user ordinals are {ordinals}"
                )
                continue
            packet_users = packet_trigger.get("users", [])
            if len(ordered_models) != len(packet_users):
                failures.append(
                    f"{config} at {commit_time}: model has {len(ordered_models)} "
                    f"users but PCAP has {len(packet_users)}"
                )
                continue
            for model, packet in zip(ordered_models, packet_users):
                try:
                    decoded_tone_size, decoded_tone_offset = decode_he_trigger_ru(
                        int(packet_trigger["ul_bandwidth"]),
                        int(packet["ru_allocation"]),
                        int(packet.get("ru_allocation_region") or 0),
                    )
                except (KeyError, TypeError, ValueError) as error:
                    inconclusive.append(f"{config} at {commit_time}: {error}")
                    continue
                matches = (
                    int(model["association_id"]) == int(packet["association_id"])
                    and int(model["ru_tone_size"]) == decoded_tone_size
                    and int(model["ru_tone_offset"]) == decoded_tone_offset
                )
                observation = {
                    "config": config,
                    "simulation_time": str(commit_time),
                    "pcap_simulation_time": str(capture_time),
                    "commit_to_capture_delay_us": float(
                        capture_delay * Decimal("1000000")
                    ),
                    "trigger_id": int(model["trigger_id"]),
                    "user_ordinal": int(model["user_ordinal"]),
                    "association_id": int(model["association_id"]),
                    "backlog_bytes": int(model["backlog_bytes"]),
                    "reported_bytes": int(model["reported_bytes"]),
                    "planned_bytes": int(model["planned_bytes"]),
                    "tid": int(model["tid"]),
                    "access_category": int(model["access_category"]),
                    "selected": bool(model["selected"]),
                    "model_ru_index": int(model["ru_index"]),
                    "model_ru_tone_size": int(model["ru_tone_size"]),
                    "model_ru_tone_offset": int(model["ru_tone_offset"]),
                    "pcap_frame_number": int(packet_trigger["frame_number"]),
                    "pcap_ru_allocation": int(packet["ru_allocation"]),
                    "pcap_ru_tone_size": decoded_tone_size,
                    "pcap_ru_tone_offset": decoded_tone_offset,
                    "matched": matches,
                }
                observations.append(observation)
                if not matches:
                    failures.append(
                        f"{config} at {commit_time} user {model['user_ordinal']}: "
                        "AID/RU allocation mismatch"
                    )
    if failures:
        return Evaluation("FAIL", "; ".join(failures[:8]), observations)
    if inconclusive:
        return Evaluation(
            "INCONCLUSIVE", "; ".join(inconclusive[:8]), observations
        )
    if not observations:
        return Evaluation(
            "INCONCLUSIVE", "No one-to-one Basic Trigger user joins were produced.", []
        )
    return Evaluation(
        "PASS",
        f"All {len(observations)} committed Basic Trigger user allocations "
        "match decoded PCAP AID/RU fields one-to-one.",
        observations,
    )


def _single_condition(conditions: Iterable[Condition], config: str) -> Condition:
    matches = [condition for condition in conditions if condition.config == config]
    if len(matches) != 1:
        raise RuntimeError(f"Expected exactly one condition for {config}, found {len(matches)}")
    return matches[0]


def _aligned_mimo_samples(
    condition: Condition, evaluation: dict[str, Any]
) -> dict[int, list[tuple[float, int, int, int]]]:
    frames = [
        condition.vectors(evaluation[key], module=evaluation["module"])
        for key in ("station_id", "stream_count", "stream_start")
    ]
    per_run: dict[int, list[tuple[float, int, int, int]]] = {}
    for run_number in sorted({pair.run_number for pair in condition.result_files}):
        columns: list[tuple[np.ndarray, np.ndarray]] = []
        for frame in frames:
            rows = frame[frame.runnumber.astype(int) == run_number]
            if rows.empty:
                raise RuntimeError(f"{condition.config}: missing MU-MIMO telemetry for run {run_number}")
            samples = [
                crop_vector(row.vectime, row.vecvalue, condition.measurement)
                for _, row in rows.iterrows()
            ]
            times = np.concatenate([item[0] for item in samples])
            values = np.concatenate([item[1] for item in samples])
            order = np.argsort(times, kind="stable")
            columns.append((times[order], values[order]))
        reference_times = columns[0][0]
        if any(
            len(times) != len(reference_times)
            or not np.array_equal(times, reference_times)
            for times, _ in columns[1:]
        ):
            raise RuntimeError(
                f"{condition.config}: cropped MU-MIMO vectors are not timestamp-aligned "
                f"for run {run_number}"
            )
        per_run[run_number] = [
            (float(timestamp), int(station), int(count), int(start))
            for timestamp, station, count, start in zip(
                reference_times, columns[0][1], columns[1][1], columns[2][1]
            )
        ]
    return per_run


def _delivery_by_run_seed(
    condition: Condition, result_name: str
) -> dict[tuple[int, int], float]:
    frame = app_sink_vectors(condition, result_name)
    if "seedset" not in frame:
        raise RuntimeError(f"{condition.config}/{result_name}: seedset metadata is missing")
    delivered: dict[tuple[int, int], float] = {}
    for run_number, rows in frame.groupby("runnumber"):
        seed_sets = set(rows.seedset.astype(int))
        if len(seed_sets) != 1:
            raise RuntimeError(
                f"{condition.config} run {run_number}: expected one recorded seed set"
            )
        total = sum(
            float(np.sum(crop_vector(row.vectime, row.vecvalue, condition.measurement)[1]))
            for _, row in rows.iterrows()
        )
        delivered[(int(run_number), seed_sets.pop())] = total
    return delivered


def _trigger_decision_rows(
    condition: Condition, evaluation: dict[str, Any]
) -> list[dict[str, Any]]:
    run_number = int(evaluation["diagnostic_run"])
    columns = {}
    for key, name in evaluation["vectors"].items():
        expression = f'module =~ "{evaluation["module"]}" AND name =~ "{name}"'
        frame = scave_results.get_vectors(
            condition._read(expression),
            omit_empty_vectors=True,
            **QUERY_OPTIONS,
        )
        if frame.empty:
            raise MissingDiagnosticTelemetryError(
                condition.config, name, run_number
            )
        frame = frame[frame.runnumber.astype(int) == run_number]
        if frame.empty:
            raise MissingDiagnosticTelemetryError(
                condition.config, name, run_number
            )
        if len(frame) != 1:
            raise RuntimeError(
                f"{condition.config}/{name}: expected one diagnostic vector "
                f"for run {run_number}, found {len(frame)}"
            )
        row = frame.iloc[0]
        columns[key] = (
            np.asarray(row.vectime, dtype=float),
            np.asarray(row.vecvalue, dtype=float),
        )
    reference_times = columns["trigger_id"][0]
    if any(
        len(times) != len(reference_times)
        or not np.array_equal(times, reference_times)
        for times, _ in columns.values()
    ):
        raise RuntimeError(
            f"{condition.config}: Trigger decision projection vectors are not aligned"
        )
    return [
        {
            "simulation_time": float(reference_times[index]),
            **{
                key: values[index]
                for key, (_, values) in columns.items()
            },
        }
        for index in range(len(reference_times))
    ]


def _bsr_decision_rows(
    condition: Condition, evaluation: dict[str, Any]
) -> dict[int, list[dict[str, Any]]]:
    """Load the aligned per-user scheduler decision projections for every run."""
    rows_by_run: dict[int, list[dict[str, Any]]] = {}
    for run_number in sorted({item.run_number for item in condition.result_files}):
        columns = {}
        for key, name in evaluation["vectors"].items():
            expression = f'module =~ "{evaluation["module"]}" AND name =~ "{name}"'
            frame = scave_results.get_vectors(
                condition._read(expression),
                omit_empty_vectors=True,
                **QUERY_OPTIONS,
            )
            frame = frame[frame.runnumber.astype(int) == run_number]
            if frame.empty:
                raise MissingDiagnosticTelemetryError(condition.config, name, run_number)
            if len(frame) != 1:
                raise RuntimeError(
                    f"{condition.config}/{name}: expected one decision vector "
                    f"for run {run_number}, found {len(frame)}"
                )
            row = frame.iloc[0]
            columns[key] = (
                np.asarray(row.vectime, dtype=float),
                np.asarray(row.vecvalue, dtype=float),
            )
        reference_times = columns["trigger_id"][0]
        if any(
            len(times) != len(reference_times)
            or not np.array_equal(times, reference_times)
            for times, _ in columns.values()
        ):
            raise RuntimeError(
                f"{condition.config} run {run_number}: BSR decision vectors are not aligned"
            )
        rows_by_run[run_number] = [
            {
                "simulation_time": float(reference_times[index]),
                **{
                    key: values[index]
                    for key, (_, values) in columns.items()
                },
            }
            for index in range(len(reference_times))
        ]
    return rows_by_run


def evaluate_bsr_decision_join(
    model_rows: dict[str, dict[int, list[dict[str, Any]]]],
) -> Evaluation:
    """Verify that reported and planned bytes share aligned trigger decisions."""
    observations = []
    failures = []
    for config, runs in sorted(model_rows.items()):
        for run_number, rows in sorted(runs.items()):
            if not rows:
                failures.append(f"{config} run {run_number}: no BSR decisions")
                continue
            grouped: dict[tuple[float, int], list[dict[str, Any]]] = {}
            for row in rows:
                key = (float(row["simulation_time"]), int(row["trigger_id"]))
                grouped.setdefault(key, []).append(row)
            for (timestamp, trigger_id), users in sorted(grouped.items()):
                if any(
                    not all(math.isfinite(float(row[key])) for key in ("reported_bytes", "planned_bytes"))
                    for row in users
                ):
                    failures.append(
                        f"{config} run {run_number} trigger {trigger_id}: non-finite backlog projection"
                    )
                    continue
                observations.append({
                    "config": config,
                    "run_number": run_number,
                    "simulation_time": timestamp,
                    "trigger_id": trigger_id,
                    "user_count": len(users),
                    "reported_bytes": int(sum(float(row["reported_bytes"]) for row in users)),
                    "planned_bytes": int(sum(float(row["planned_bytes"]) for row in users)),
                })
    if failures:
        return Evaluation("FAIL", "; ".join(failures[:8]), observations)
    if not observations:
        return Evaluation("INCONCLUSIVE", "No aligned BSR scheduler decisions were retained.", [])
    return Evaluation(
        "PASS",
        "Reported and planned backlog bytes are aligned to a trigger decision ID for every retained decision.",
        observations,
    )


def _packet_trigger_rows(
    evaluation: dict[str, Any], requested_session_id: str,
) -> dict[str, list[dict[str, Any]]]:
    path = REPOSITORY_ROOT / evaluation["packet_metrics"]
    if not path.is_file():
        raise FileNotFoundError(path)
    document = json.loads(path.read_text(encoding="utf-8"))
    packet_session = document.get("_provenance", {}).get("capture_session_id")
    if packet_session != requested_session_id:
        raise RuntimeError(
            f"Packet metrics session {packet_session!r} does not match "
            f"scalar/vector session {requested_session_id!r}"
        )
    run_number = document.get("_provenance", {}).get("run_number")
    if run_number != int(evaluation["diagnostic_run"]):
        raise RuntimeError(
            f"Packet metrics run {run_number!r} does not match diagnostic run "
            f"{evaluation['diagnostic_run']}"
        )
    group = document.get("ul_ofdma", {})
    return {
        config: group.get(config, {}).get("global", {}).get(
            "he_trigger_allocations", []
        )
        for config in evaluation["configs"]
    }


def evaluate_contract(
    contract: dict[str, Any], conditions: list[Condition],
    requested_session_id: str | None = None,
) -> tuple[Evaluation, list[dict[str, str]]]:
    evaluation = contract["evaluation"]
    handler = evaluation["handler"]
    if handler == "unimplemented":
        return Evaluation("INCONCLUSIVE", evaluation["reason"], []), []
    if handler == "mimo_disjoint_streams":
        condition = _single_condition(conditions, evaluation["config"])
        result = evaluate_mimo_triplets(_aligned_mimo_samples(condition, evaluation))
        filters = [
            {"type": "vector", "module": evaluation["module"], "name": evaluation[key]}
            for key in ("station_id", "stream_count", "stream_start")
        ]
        return result, filters
    if handler == "matched_delivery_ratio":
        baseline = _single_condition(conditions, evaluation["baseline_config"])
        treatment = _single_condition(conditions, evaluation["treatment_config"])
        result = evaluate_matched_delivery(
            _delivery_by_run_seed(baseline, evaluation["result"]),
            _delivery_by_run_seed(treatment, evaluation["result"]),
            float(evaluation["minimum_ratio"]),
        )
        return result, [{
            "type": "vector",
            "module": "**.app[*]",
            "name": evaluation["result"],
            "selection": "condition sink_module_regex",
        }]
    if handler == "ul_trigger_allocation_join":
        if requested_session_id is None:
            raise RuntimeError(
                "UL Trigger allocation join requires a resolved result session"
            )
        selected = {
            config: _single_condition(conditions, config)
            for config in evaluation["configs"]
        }
        model_rows = {
            config: _trigger_decision_rows(condition, evaluation)
            for config, condition in selected.items()
        }
        packet_rows = _packet_trigger_rows(evaluation, requested_session_id)
        result = evaluate_ul_trigger_allocation_join(model_rows, packet_rows)
        filters = [
            {
                "type": "vector",
                "module": evaluation["module"],
                "name": name,
                "run": str(evaluation["diagnostic_run"]),
            }
            for name in evaluation["vectors"].values()
        ]
        filters.append({
            "type": "packet",
            "module": "AP PCAP",
            "name": (
                "wlan.trigger.he.user_info.aid12,"
                "wlan.trigger.he.ru_allocation"
            ),
            "run": str(evaluation["diagnostic_run"]),
        })
        return result, filters
    if handler == "bsr_decision_join":
        selected = {
            config: _single_condition(conditions, config)
            for config in evaluation["configs"]
        }
        model_rows = {
            config: _bsr_decision_rows(condition, evaluation)
            for config, condition in selected.items()
        }
        result = evaluate_bsr_decision_join(model_rows)
        filters = [
            {
                "type": "vector",
                "module": evaluation["module"],
                "name": name,
            }
            for name in evaluation["vectors"].values()
        ]
        return result, filters
    raise RuntimeError(f"Unknown evidence handler {handler!r}")


def build_ledger(
    manifest: dict[str, Any],
    requested_session_id: str | None,
    selected_groups: list[str],
    manifest_path: Path = DEFAULT_MANIFEST,
) -> dict[str, Any]:
    groups: dict[str, Any] = {}
    all_checks: list[dict[str, Any]] = []
    for group_name in selected_groups:
        group = manifest["groups"][group_name]
        try:
            selected_session = resolve_session_id(group, requested_session_id)
        except FileNotFoundError as error:
            checks = []
            for contract in manifest["evidence_contracts"][group_name]:
                check = {
                    **{key: contract[key] for key in ("id", "kind", "requirement", "results")},
                    "handler": contract["evaluation"]["handler"],
                    **asdict(Evaluation("NOT RUN", str(error), [])),
                    "filters": [],
                    "measurement": group["measurement"],
                }
                checks.append(check)
                all_checks.append(check)
            groups[group_name] = {
                "status": "NOT RUN",
                "session_id": None,
                "reason": str(error),
                "checks": checks,
            }
            continue
        conditions = conditions_for_group(manifest, group_name, selected_session)
        checks = []
        for contract in manifest["evidence_contracts"][group_name]:
            try:
                result, filters = evaluate_contract(
                    contract, conditions, selected_session
                )
            except MissingDiagnosticTelemetryError as error:
                result = Evaluation("NOT RUN", str(error), [])
                filters = []
            check = {
                **{key: contract[key] for key in ("id", "kind", "requirement", "results")},
                "handler": contract["evaluation"]["handler"],
                **asdict(result),
                "filters": filters,
                "measurement": {
                    "start_s": conditions[0].measurement.start,
                    "end_s": conditions[0].measurement.end,
                },
            }
            checks.append(check)
            all_checks.append(check)
        counts = Counter(check["status"] for check in checks)
        groups[group_name] = {
            "status": (
                "FAIL" if counts["FAIL"] else
                "INCONCLUSIVE" if counts["INCONCLUSIVE"] else
                "NOT RUN" if counts["NOT RUN"] else "PASS"
            ),
            "session_id": selected_session,
            "provenance": {
                "conditions": [condition.provenance() for condition in conditions],
            },
            "checks": checks,
        }
    summary = {status: 0 for status in sorted(STATUSES)}
    summary.update(Counter(check["status"] for check in all_checks))
    return {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "repository_revision": git_revision(),
        "manifest": (
            str(manifest_path.relative_to(REPOSITORY_ROOT))
            if manifest_path.is_relative_to(REPOSITORY_ROOT)
            else str(manifest_path)
        ),
        "requested_session_id": requested_session_id,
        "groups": groups,
        "status_summary": summary,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--session-id", help="evaluate this YYYYMMDDTHHMMSSZ result session")
    parser.add_argument("--group", action="append", help="evaluate one group; repeat as needed")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--require-conclusive",
        action="store_true",
        help="return failure unless every selected check is PASS",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.session_id is not None and not SESSION_ID_PATTERN.fullmatch(args.session_id):
        raise ValueError("Session ID must use YYYYMMDDTHHMMSSZ")
    manifest = load_manifest(args.manifest)
    selected_groups = args.group or list(manifest["groups"])
    unknown = set(selected_groups) - set(manifest["groups"])
    if unknown:
        raise ValueError(f"Unknown analysis groups: {sorted(unknown)}")
    ledger = build_ledger(
        manifest, args.session_id, selected_groups, args.manifest
    )
    output = args.output if args.output.is_absolute() else REPOSITORY_ROOT / args.output
    atomic_write_text(
        output, json.dumps(ledger, indent=2, sort_keys=True, allow_nan=False) + "\n"
    )
    try:
        display_output = output.relative_to(REPOSITORY_ROOT)
    except ValueError:
        display_output = output
    print(f"CREATED {display_output}")
    summary = ledger["status_summary"]
    if summary["FAIL"] or (
        args.require_conclusive
        and (summary["INCONCLUSIVE"] or summary["NOT RUN"])
    ):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
