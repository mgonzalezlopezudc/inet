#!/usr/bin/env python3
"""Write machine-readable per-condition metrics for the checked-in analyses."""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np

from analysis_core import (
    DEFAULT_MANIFEST,
    REPOSITORY_ROOT,
    conditions_for_group,
    crop_vector,
    load_manifest,
    per_run_delay_percentile,
    per_run_goodput,
    summarize_ci95,
)
from analysis_plots import _ap_overlap, _energy_per_run, _per_run_fairness


def ci(values) -> dict:
    return summarize_ci95(np.asarray(values, dtype=float))


def main() -> None:
    manifest = load_manifest(DEFAULT_MANIFEST)
    output = REPOSITORY_ROOT / "examples/ieee80211ax/analysis/metrics.json"
    payload: dict[str, dict] = {}
    if output.exists():
        try:
            payload = json.loads(output.read_text())
        except Exception:
            pass
    for group_name in manifest["groups"]:
        try:
            conditions = conditions_for_group(manifest, group_name)
        except Exception as e:
            print(f"Skipping group '{group_name}' (results not found/incomplete): {e}")
            continue
        group_metrics: dict[str, dict] = {}
        for condition in conditions:
            item: dict = {}
            try:
                goodput = per_run_goodput(condition)
                item["goodput_mbps"] = ci(goodput.goodput_bps / 1e6)
            except RuntimeError:
                pass
            if group_name in {"width", "dl"}:
                item["delay_p95_ms"] = ci(per_run_delay_percentile(condition, 95).delay_s * 1e3)
            if group_name in {"bss", "dl"}:
                normalized = group_name == "dl" and condition.condition_metadata.get("workload") == "asymmetric"
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
                attempts = attempts[attempts.module.str.contains(".host[", regex=False)].groupby("runnumber").value.sum()
                successes = successes[successes.module.str.contains(".host[", regex=False)].groupby("runnumber").value.sum()
                item["attempts"] = ci(attempts)
                item["success_probability"] = ci(successes / attempts)
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
                for result_name, vector_name in (
                    ("reported_backlog_mean_bytes", "heUlBufferStatusReportedBytes:vector"),
                    ("scheduled_backlog_mean_bytes", "heUlBufferStatusScheduledBytes:vector"),
                ):
                    vectors = condition.vectors(vector_name)
                    item[result_name] = ci([
                        np.mean(np.concatenate([
                            crop_vector(row.vectime, row.vecvalue, condition.measurement)[1]
                            for _, row in rows.iterrows()
                        ]))
                        for _, rows in vectors.groupby("runnumber")
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
    output = REPOSITORY_ROOT / "examples/ieee80211ax/analysis/metrics.json"
    output.write_text(json.dumps(payload, indent=2, sort_keys=True, allow_nan=False) + "\n")
    print(f"CREATED {output}")


if __name__ == "__main__":
    main()
