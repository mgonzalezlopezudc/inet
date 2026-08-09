#!/usr/bin/env python3
"""Write machine-readable per-condition metrics for the checked-in analyses."""

from __future__ import annotations

import argparse
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np

from analysis_core import (
    DEFAULT_MANIFEST,
    REPOSITORY_ROOT,
    atomic_write_text,
    conditions_for_group,
    crop_vector,
    jain,
    load_manifest,
    per_run_delay_percentile,
    per_run_goodput,
    git_revision,
    resolve_manifest_sessions,
    result_session_directory,
    summarize_ci95,
)
from analysis_plots import _ap_overlap, _energy_per_run, _per_run_fairness


def ci(values) -> dict:
    summary = summarize_ci95(np.asarray(values, dtype=float))
    if not math.isfinite(float(summary["ci95"])):
        summary["ci95"] = None
    return summary


def time_weighted_step_mean(times, values, measurement) -> float:
    """Average an event-driven, post-step state over a measurement window."""
    times = np.asarray(times, dtype=float)
    values = np.asarray(values, dtype=float)
    initial_index = np.searchsorted(times, measurement.start, side="right") - 1
    if initial_index < 0:
        raise RuntimeError("step vector has no state at the measurement-window start")
    following_times = times[initial_index + 1:]
    following_values = values[initial_index + 1:]
    inside = following_times < measurement.end
    boundaries = np.concatenate((
        [measurement.start],
        following_times[inside],
        [measurement.end],
    ))
    states = np.concatenate(([values[initial_index]], following_values[inside]))
    return float(np.sum(states * np.diff(boundaries)) /
                 (measurement.end - measurement.start))


def natural_sort_key(s: str) -> list[object]:
    return [int(text) if text.isdigit() else text for text in re.split(r'(\d+)', str(s))]


def condition_sort_key(s: str) -> list[object]:
    name = str(s)
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
    if name in ul_ofdma_order:
        return [ul_ofdma_order[name]]
    if "2.5ms" in name:
        rank = 1
    elif "2.0ms" in name or "2ms" in name:
        rank = 2
    elif "1.5ms" in name:
        rank = 3
    else:
        rank = 0
    return [rank] + natural_sort_key(name)


def sort_payload_keys(obj: Any) -> Any:
    if isinstance(obj, dict):
        sorted_keys = sorted(
            obj.keys(),
            key=lambda k: (0 if k == "_provenance" else 1, condition_sort_key(k)),
        )
        return {k: sort_payload_keys(obj[k]) for k in sorted_keys}
    if isinstance(obj, list):
        return [sort_payload_keys(item) for item in obj]
    return obj


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument(
        "--group",
        help="summarize one manifest group (default: every group)",
    )
    parser.add_argument(
        "--session-id",
        help="use this YYYYMMDDTHHMMSSZ result session instead of the latest complete one",
    )
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    if args.group is not None and args.group not in manifest["groups"]:
        parser.error(
            f"unknown group {args.group!r}; choose from: "
            + ", ".join(sorted(manifest["groups"]))
        )
    if args.group is not None:
        manifest = {
            **manifest,
            "groups": {args.group: manifest["groups"][args.group]},
            "evidence_contracts": {
                args.group: manifest["evidence_contracts"][args.group]
            },
        }
    selected_groups = list(manifest["groups"])
    output = REPOSITORY_ROOT / "examples/ieee80211ax/analysis/metrics.json"
    selected_sessions, unavailable_sessions = resolve_manifest_sessions(
        manifest, args.session_id
    )
    payload: dict[str, dict] = {
        "_provenance": {
            "schema_version": 1,
            "generated_at_utc": datetime.now(timezone.utc).isoformat(),
            "repository_revision": git_revision(),
            "requested_session_id": args.session_id,
            "groups": {},
        }
    }
    for group_name in selected_groups:
        if group_name not in selected_sessions:
            reason = unavailable_sessions[group_name]
            payload["_provenance"]["groups"][group_name] = {
                "status": "NOT RUN",
                "reason": reason,
            }
            print(f"Skipping group '{group_name}' (NOT RUN): {reason}")
            continue
        selected_session = selected_sessions[group_name]
        conditions = conditions_for_group(
            manifest, group_name, selected_session
        )
        payload["_provenance"]["groups"][group_name] = {
            "status": "PASS",
            "session_id": selected_session,
            "conditions": [condition.provenance() for condition in conditions],
        }
        group_metrics: dict[str, dict] = {}
        for condition in conditions:
            item: dict = {}
            try:
                goodput = per_run_goodput(condition)
                item["goodput_mbps"] = ci(goodput.goodput_bps / 1e6)
            except RuntimeError:
                pass
            if group_name in {"uora", "width", "dl_sched", "dl_asym", "ul_ofdma", "dl_ul_ofdma"}:
                item["delay_p95_ms"] = ci(per_run_delay_percentile(condition, 95).delay_s * 1e3)
            if group_name in {"bss", "dl_sched", "dl_asym"}:
                normalized = group_name == "dl_asym" or condition.condition_metadata.get("workload") == "asymmetric"
                item["jain_fairness"] = ci(_per_run_fairness(condition, normalized=normalized).fairness)
            if group_name == "bss":
                item["concurrent_ap_airtime_percent"] = ci(_ap_overlap(condition).overlap * 100)
            if group_name == "twt":
                energy = _energy_per_run(condition)
                item["energy_per_bit_j"] = ci(energy.energy_per_bit)
                item["delivered_bytes"] = ci(energy.delivered_bytes)
            if group_name == "uora":
                attempts = condition.scalars("heUlRandomAccessAttempt:count")
                successes = condition.scalars("heUlRandomAccessSuccess:count")
                attempts = attempts[attempts.module.str.contains(".host[", regex=False)]
                successes = successes[successes.module.str.contains(".host[", regex=False)]
                attempt_totals = attempts.groupby("runnumber").value.sum()
                success_totals = successes.groupby("runnumber").value.sum()
                item["attempts"] = ci(attempt_totals)
                item["successful_transmissions"] = ci(success_totals)
                positive_attempts = attempt_totals > 0
                if positive_attempts.any():
                    item["success_probability"] = ci(
                        success_totals[positive_attempts]
                        / attempt_totals[positive_attempts]
                    )
                fairness = [
                    jain(rows.value)
                    for _, rows in successes.groupby("runnumber")
                ]
                item["zero_success_run_count"] = sum(
                    math.isnan(value) for value in fairness
                )
                defined_fairness = [
                    value for value in fairness if not math.isnan(value)
                ]
                if defined_fairness:
                    item["success_fairness"] = ci(defined_fairness)
            if group_name == "fragmentation":
                sizes = condition.vectors(
                    "packetSentToPeer:vector(packetBytes)",
                    module="**.host[*].wlan[0].mac.hcf",
                )
                airtime = condition.vectors(
                    "acknowledgmentAirtime:vector", module="**.radio", expected_unit="s"
                )
                item["mac_frame_size_mean_bytes"] = ci([
                    np.mean(np.concatenate([
                        crop_vector(row.vectime, row.vecvalue, condition.measurement)[1]
                        for _, row in rows.iterrows()
                    ]))
                    for _, rows in sizes.groupby("runnumber")
                ])
                item["ack_airtime_total_ms"] = ci([
                    1e3 * sum(np.sum(crop_vector(row.vectime, row.vecvalue, condition.measurement)[1])
                               for _, row in rows.iterrows())
                    for _, rows in airtime.groupby("runnumber")
                ])
            if group_name == "bsr":
                vectors = condition.vectors(
                    "heUlBufferStatusReportedBytes:vector",
                    module="**.ap.wlan[0].mac.hcf.ulCoordinator",
                )
                item["reported_backlog_time_weighted_mean_bytes"] = ci([
                    time_weighted_step_mean(
                        row.vectime, row.vecvalue, condition.measurement
                    )
                    for _, row in vectors.iterrows()
                ])
            if group_name == "rate":
                mcs = condition.vectors("heRateSelectedMcs:vector", module="**.rateControl")
                outcomes = condition.vectors("heRateTxSuccess:vector", module="**.rateControl")
                item["selected_mcs_min"] = int(min(np.min(row.vecvalue) for _, row in mcs.iterrows()))
                item["selected_mcs_max"] = int(max(np.max(row.vecvalue) for _, row in mcs.iterrows()))
                item["tx_success_fraction"] = ci([
                    np.mean(np.concatenate(rows.vecvalue.to_list()))
                    for _, rows in outcomes.groupby("runnumber")
                ])
            if group_name == "puncturing" and condition.config == "DynamicPuncturing":
                masks = condition.vectors("hePuncturedSubchannelMask:vector", module="**.ap.wlan[0].radio")
                item["observed_masks"] = sorted({int(value) for row in masks.vecvalue for value in row})
            group_metrics[condition.label] = item
        if group_name == "twt":
            baseline = _energy_per_run(conditions[0]).set_index("runnumber").delivered_bytes
            treatment = _energy_per_run(conditions[1]).set_index("runnumber").delivered_bytes
            group_metrics["delivery_ratio_twt_over_baseline"] = ci(treatment / baseline)
        payload[group_name] = group_metrics
    serialized = json.dumps(
        sort_payload_keys(payload), indent=2, sort_keys=False, allow_nan=False
    ) + "\n"
    atomic_write_text(output, serialized)
    if args.group is not None:
        selected_session = payload["_provenance"]["groups"][args.group].get(
            "session_id"
        )
        if selected_session:
            session_output = result_session_directory(
                manifest["groups"][args.group], selected_session
            ) / "metrics.json"
            atomic_write_text(session_output, serialized)
            print(f"CREATED {session_output}")
    print(f"CREATED {output}")


if __name__ == "__main__":
    main()
