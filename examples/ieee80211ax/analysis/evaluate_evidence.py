#!/usr/bin/env python3
"""Evaluate executable scalar/vector evidence contracts into a durable ledger."""

from __future__ import annotations

import argparse
import json
import math
from collections import Counter
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

import numpy as np

from analysis_core import (
    DEFAULT_MANIFEST,
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


def evaluate_contract(
    contract: dict[str, Any], conditions: list[Condition]
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
            result, filters = evaluate_contract(contract, conditions)
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
