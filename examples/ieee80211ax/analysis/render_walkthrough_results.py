#!/usr/bin/env python3
"""Render scalar/vector metric tables and figures into walkthrough sections."""

from __future__ import annotations

import argparse
import json
import math
import os
import re
from pathlib import Path
from typing import Any

from analysis_core import (
    DEFAULT_MANIFEST,
    FIGURE_FILENAMES,
    REPOSITORY_ROOT,
    atomic_write_text,
    load_manifest,
    result_session_directory,
)
from inet_wifi_analysis import (
    normalize_heading_label,
    update_script_results_session,
)


DEFAULT_METRICS = Path(__file__).resolve().parent / "metrics.json"
DEFAULT_EVIDENCE_LEDGER = (
    Path(__file__).resolve().parent / "evidence-ledger.json"
)


def escape_cell(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def format_number(value: object) -> str:
    if value is None:
        return "N/A"
    if isinstance(value, bool):
        return str(value)
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        if not math.isfinite(value):
            return "N/A"
        return f"{value:.6g}"
    return escape_cell(value)


def format_mw_as_dbm(value: object) -> str:
    if value is None:
        return "N/A"
    milliwatts = float(value)
    if not math.isfinite(milliwatts) or milliwatts <= 0:
        return "N/A"
    return f"{10 * math.log10(milliwatts):.2f} dBm"


def metric_label(name: str) -> str:
    return name.replace("_", " ")


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
    bsr_order = {
        "FreshBsr": 0,
        "StaleBsr": 1,
        "LongTriggerCheck": 2,
        "BurstyTraffic": 3,
    }
    if name in bsr_order:
        return [bsr_order[name]]
    if "2.5ms" in name:
        rank = 1
    elif "2.0ms" in name or "2ms" in name:
        rank = 2
    elif "1.5ms" in name:
        rank = 3
    else:
        rank = 0
    return [rank] + natural_sort_key(name)


def metric_rows(group_metrics: dict[str, Any]) -> list[list[str]]:
    rows: list[list[str]] = []
    for condition, metrics in sorted(group_metrics.items(), key=lambda item: condition_sort_key(item[0])):
        if not isinstance(metrics, dict):
            rows.append([
                condition, "direct value", "—", format_number(metrics), "—",
            ])
            continue
        for metric, summary in metrics.items():
            if isinstance(summary, dict) and {"count", "mean"} <= summary.keys():
                rows.append([
                    condition,
                    metric_label(metric),
                    format_number(summary["count"]),
                    format_number(summary["mean"]),
                    format_number(summary.get("ci95", "N/A")),
                ])
            else:
                rows.append([
                    condition,
                    metric_label(metric),
                    "—",
                    format_number(summary),
                    "—",
                ])
    return rows


def render_evidence_markdown(
    group_name: str, group_evidence: dict[str, Any], session_id: str
) -> str:
    if group_evidence.get("session_id") != session_id:
        raise RuntimeError(
            f"{group_name}: evidence session "
            f"{group_evidence.get('session_id')!r} does not match metrics "
            f"session {session_id!r}"
        )
    lines = [
        "\n### [script] Executable evidence checks\n\n",
        "| Status | Requirement | Evaluation |\n",
        "|---|---|---|\n",
    ]
    for check in group_evidence.get("checks", []):
        lines.append(
            f"| **{escape_cell(check['status'])}** | "
            f"{escape_cell(check['requirement'])} | "
            f"{escape_cell(check['reason'])} |\n"
        )
    allocation_check = next(
        (
            check for check in group_evidence.get("checks", [])
            if check.get("handler") == "ul_trigger_allocation_join"
        ),
        None,
    )
    if allocation_check is not None:
        lines.extend([
            "\n#### [script] Joined Basic Trigger allocation evidence\n\n",
            "| Config | Time (s) / Trigger / user | AID | "
            "Reported / planned bytes | Model RU | PCAP RU field → decoded RU | Match |\n",
            "|---|---|---:|---:|---|---|---|\n",
        ])
        observations = allocation_check.get("observations", [])
        for row in observations[:12]:
            lines.append(
                f"| {escape_cell(row['config'])} | "
                f"{escape_cell(row['simulation_time'])} / "
                f"{row['trigger_id']} / {row['user_ordinal']} | "
                f"{row['association_id']} | "
                f"{row['reported_bytes']} / {row['planned_bytes']} | "
                f"{row['model_ru_tone_size']}@{row['model_ru_tone_offset']} | "
                f"{row['pcap_ru_allocation']} → "
                f"{row['pcap_ru_tone_size']}@{row['pcap_ru_tone_offset']} | "
                f"{'PASS' if row['matched'] else 'FAIL'} |\n"
            )
        if len(observations) > 12:
            lines.append(
                f"\nShowing 12 of {len(observations)} joined users; the "
                "session-bound evidence ledger retains every observation.\n"
            )
    bsr_check = next(
        (
            check for check in group_evidence.get("checks", [])
            if check.get("handler") == "bsr_decision_join"
        ),
        None,
    )
    if bsr_check is not None:
        observations = bsr_check.get("observations", [])
        for config in sorted({row["config"] for row in observations}, key=condition_sort_key):
            config_observations = [
                row for row in observations if row["config"] == config
            ]
            lines.extend([
                f"\n#### [script] Joined BSR scheduler-decision evidence: "
                f"{escape_cell(config)}\n\n",
                "| Config | Run | Time (s) / Trigger | Users | Reported bytes | Planned bytes |\n",
                "|---|---:|---:|---:|---:|---:|\n",
            ])
            for row in config_observations[:100]:
                lines.append(
                    f"| {escape_cell(row['config'])} | {row['run_number']} | "
                    f"{row['simulation_time']} / {row['trigger_id']} | "
                    f"{row['user_count']} | {row['reported_bytes']} | "
                    f"{row['planned_bytes']} |\n"
                )
            if len(config_observations) > 100:
                lines.append(
                    f"\nShowing 100 of {len(config_observations)} joined decisions "
                    f"for {escape_cell(config)}; the session-bound evidence "
                    "ledger retains every observation.\n"
                )
    bss_check = next(
        (
            check for check in group_evidence.get("checks", [])
            if check.get("handler") == "bss_spatial_reuse_join"
        ),
        None,
    )
    if bss_check is not None:
        for observation in bss_check.get("observations", []):
            trace = observation.get("representative_ap1_decisions", [])
            if not trace:
                continue
            lines.extend([
                f"\n#### [script] Representative AP decisions: {escape_cell(observation['config'])}\n\n",
                "The rows are ten evenly spaced AP1 run-0 decision samples. "
                "Treatments sample eligible inter-BSS decisions; controls sample all "
                "retained decisions. Received power is full-channel power; the receiver "
                "applies its RU-aware OBSS/PD test before recording the outcome.\n\n",
                "| Time (s) | Local / PPDU color | BSS class | Eligible | PPDU power | Configured OBSS/PD | Reason | Ignore PPDU | TX power limit |\n",
                "|---:|---|---|---|---:|---:|---|---|---:|\n",
            ])
            for row in trace:
                bss_class = {
                    0: "unspecified",
                    1: "intra-BSS",
                    2: "inter-BSS non-SRG",
                    3: "inter-BSS SRG",
                }.get(row["bss_type"], "unknown")
                reason = {
                    0: "spatial reuse disabled",
                    1: "not an HE PPDU",
                    2: "received color disabled",
                    3: "local color disabled",
                    4: "intra-BSS PPDU",
                    11: "below OBSS/PD",
                    12: "at/above OBSS/PD",
                }.get(row["reason"], "unknown")
                lines.append(
                    f"| {row['simulation_time']:.9f} | "
                    f"{row['local_bss_color']} / {row['received_bss_color']} | "
                    f"{bss_class} | {'yes' if row['eligible'] else 'no'} | "
                    f"{format_mw_as_dbm(row['received_power_mw'])} | "
                    f"{format_mw_as_dbm(row['obss_pd_threshold_mw'])} | {reason} | "
                    f"{'yes' if row['ignored_ppdu'] else 'no'} | "
                    f"{format_mw_as_dbm(row['transmit_power_limit_mw'])} |\n"
                )
    return "".join(lines)


def relative_link(target: Path, walkthrough: Path) -> str:
    return Path(os.path.relpath(target, walkthrough.parent)).as_posix()


def source_filter_summary(sidecar: dict[str, Any]) -> str:
    filters = sidecar.get("result_filters", [])
    parts = []
    for item in filters:
        fields = [
            item.get("type"),
            item.get("module"),
            item.get("name"),
            f"unit={item['unit']}" if item.get("unit") else None,
        ]
        parts.append(" / ".join(str(value) for value in fields if value))
    return "<br>".join(parts) or "See figure provenance sidecar"


def window_aggregation_summary(sidecar: dict[str, Any]) -> str:
    windows = {
        (
            condition.get("measurement", {}).get("start_s"),
            condition.get("measurement", {}).get("end_s"),
        )
        for condition in sidecar.get("conditions", [])
    }
    window_text = ", ".join(
        f"[{start}, {end}) s" for start, end in sorted(windows)
    )
    aggregation = sidecar.get("aggregation", {})
    aggregation_text = "; ".join(
        f"{key}={value}" for key, value in aggregation.items()
    )
    return "; ".join(
        value for value in (window_text, aggregation_text) if value
    ) or "See figure provenance sidecar"


def independent_runs_summary(group_metrics: dict[str, Any]) -> str:
    counts = set()
    direct = False
    for metrics in group_metrics.values():
        if not isinstance(metrics, dict):
            direct = True
            continue
        for summary in metrics.values():
            if isinstance(summary, dict) and "count" in summary:
                counts.add(summary["count"])
            else:
                direct = True
    parts = []
    if counts:
        count_text = ", ".join(format_number(count) for count in sorted(counts))
        parts.append(f"run-level summaries: n={count_text}")
    if direct:
        parts.append("direct observations: no independent-run estimate")
    return "; ".join(parts) or "See figure provenance sidecar"


def validate_bundle_provenance(
    group_name: str,
    metrics_document: dict[str, Any],
    sidecar_document: dict[str, Any],
    figure: Path | None = None,
) -> str:
    provenance = metrics_document.get("_provenance", {})
    group_provenance = provenance.get("groups", {}).get(group_name, {})
    if group_provenance.get("status") != "PASS":
        raise RuntimeError(
            f"{group_name}: metrics have no PASS session provenance; "
            "rerun summarize_results.py"
        )
    session_id = group_provenance.get("session_id")
    if not session_id:
        raise RuntimeError(f"{group_name}: metrics provenance has no session_id")

    conditions = sidecar_document.get("conditions")
    if not isinstance(conditions, list) or not conditions:
        raise RuntimeError(f"{group_name}: figure sidecar has no conditions")
    metric_conditions = group_provenance.get("conditions")
    if not isinstance(metric_conditions, list) or not metric_conditions:
        raise RuntimeError(
            f"{group_name}: metrics provenance has no conditions"
        )
    metric_summaries = [
        {key: value for key, value in condition.items()
         if key != "result_files"}
        for condition in metric_conditions
    ]
    if metric_summaries != conditions:
        raise RuntimeError(
            f"{group_name}: metric and figure condition metadata differs"
        )
    for condition in conditions:
        if condition.get("group") != group_name:
            raise RuntimeError(
                f"{group_name}: figure sidecar contains group "
                f"{condition.get('group')!r}"
            )
    if figure is not None and figure.parent.name != session_id:
        raise RuntimeError(
            f"{group_name}: figure and metrics sessions differ; "
            f"{figure.parent.name!r} != {session_id!r}"
        )
    return session_id


def render_group(
    group_name: str,
    group: dict[str, Any],
    metrics_document: dict[str, Any],
    metrics_path: Path = DEFAULT_METRICS,
    sidecar_document: dict[str, Any] | None = None,
    evidence_document: dict[str, Any] | None = None,
) -> str:
    walkthrough = REPOSITORY_ROOT / group["walkthrough"]
    provenance = metrics_document.get("_provenance", {}).get("groups", {}).get(
        group_name, {}
    )
    session_id = provenance.get("session_id")
    if not session_id:
        raise RuntimeError(f"{group_name}: metrics provenance has no session_id")
    figure = (
        result_session_directory(group, session_id)
        / FIGURE_FILENAMES[group_name]
    )
    sidecar = figure.with_suffix(figure.suffix + ".json")
    figure_link = relative_link(figure, walkthrough)
    sidecar_link = relative_link(sidecar, walkthrough)
    if not figure.is_file():
        raise RuntimeError(f"{group_name}: figure does not exist: {figure}")
    if not sidecar.is_file():
        raise RuntimeError(
            f"{group_name}: figure provenance does not exist: {sidecar}"
        )
    if sidecar_document is None:
        sidecar_document = json.loads(sidecar.read_text(encoding="utf-8"))
    validate_bundle_provenance(
        group_name, metrics_document, sidecar_document, figure
    )
    metrics_link = relative_link(metrics_path.resolve(), walkthrough)
    source_summary = source_filter_summary(sidecar_document)
    aggregation_summary = window_aggregation_summary(sidecar_document)
    runs_summary = independent_runs_summary(metrics_document[group_name])
    figure_1ms = (
        result_session_directory(group, session_id)
        / "dl-bar-acknowledgment-dashboard-1ms.png"
    )
    figure_1ms_text = ""
    if figure_1ms.is_file():
        figure_1ms_link = relative_link(figure_1ms, walkthrough)
        figure_1ms_text = f"![{group_name} 1ms scalar/vector analysis]({figure_1ms_link})\n\n"

    lines = [
        "### [script] Generated scalar/vector plot and table\n\n",
        f"![{group_name} scalar/vector analysis]({figure_link})\n\n",
        figure_1ms_text,
        f"Figure provenance: [`{sidecar_link}`]({sidecar_link}). "
        f"Run-level metric source: [`{metrics_link}`]({metrics_link}).\n\n",
        "Common table provenance:\n\n",
        f"- Source result filters / modules / units: {source_summary}\n",
        f"- Window / per-run aggregation / exclusions: {aggregation_summary}\n",
        f"- Independent runs: {runs_summary}\n\n",
        "| Configuration / observation | Mean or direct value | "
        "95% CI half-width |\n",
        "|---|---:|---:|\n",
    ]
    for row in metric_rows(metrics_document[group_name]):
        expanded = [f"{row[0]} / {row[1]}", row[3], row[4]]
        lines.append(
            "| " + " | ".join(escape_cell(cell) for cell in expanded) + " |\n"
        )
    lines.append(
        "\nThe table is a presentation view of the session-bound run-level "
        "summary; the common provenance applies to every row.\n"
    )
    if evidence_document is not None:
        group_evidence = evidence_document.get("groups", {}).get(group_name)
        if group_evidence is not None:
            lines.append(
                render_evidence_markdown(
                    group_name, group_evidence, session_id
                )
            )
    return "".join(lines)


def replace_generated_section(
    content: str,
    marker: str,
    generated_markdown: str,
    heading: str = "Scalar and vector analysis",
) -> str:
    begin_marker = f"<!-- BEGIN GENERATED: {marker} -->"
    end_marker = f"<!-- END GENERATED: {marker} -->"
    generated = (
        f"{begin_marker}\n{generated_markdown.rstrip()}\n{end_marker}\n"
    )
    if begin_marker in content or end_marker in content:
        if content.count(begin_marker) != 1 or content.count(end_marker) != 1:
            raise ValueError(f"Malformed generated-section markers for {marker}")
        begin = content.index(begin_marker)
        end = content.index(end_marker, begin) + len(end_marker)
        return content[:begin] + generated.rstrip() + content[end:]

    headings = list(re.finditer(r"^##\s+(.+?)\s*$", content, re.MULTILINE))
    heading_index = next(
        (
            index for index, match in enumerate(headings)
            if normalize_heading_label(match.group(1)) == heading.lower()
        ),
        None,
    )
    if heading_index is None:
        raise ValueError(f"Missing walkthrough section: {heading}")
    heading_match = headings[heading_index]
    section_end = (
        headings[heading_index + 1].start()
        if heading_index + 1 < len(headings)
        else len(content)
    )
    prefix = content[:section_end].rstrip()
    suffix = content[section_end:].lstrip("\n")
    return f"{prefix}\n\n{generated}\n{suffix}".rstrip() + "\n"


def update_walkthrough(
    content: str,
    marker: str,
    generated_markdown: str,
    session_id: str,
) -> str:
    updated = replace_generated_section(content, marker, generated_markdown)
    return update_script_results_session(
        updated,
        "Scalar/vector",
        session_id,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("group", help="manifest group name, or 'all'")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--metrics", type=Path, default=DEFAULT_METRICS)
    parser.add_argument(
        "--evidence-ledger",
        type=Path,
        default=DEFAULT_EVIDENCE_LEDGER,
    )
    parser.add_argument(
        "--update",
        action="store_true",
        help="update marker-bounded blocks in the configured walkthroughs",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    manifest = load_manifest(args.manifest)
    metrics = json.loads(args.metrics.read_text(encoding="utf-8"))
    evidence = (
        json.loads(args.evidence_ledger.read_text(encoding="utf-8"))
        if args.evidence_ledger.is_file()
        else None
    )
    groups = sorted(manifest["groups"]) if args.group == "all" else [args.group]
    unknown = [name for name in groups if name not in manifest["groups"]]
    if unknown:
        raise SystemExit(f"unknown group: {unknown[0]}")

    for group_name in groups:
        group = manifest["groups"][group_name]
        if "walkthrough" not in group:
            raise RuntimeError(f"{group_name}: manifest has no walkthrough path")
        if group_name not in metrics:
            raise RuntimeError(f"{group_name}: metrics are NOT RUN or missing")
        markdown = render_group(
            group_name,
            group,
            metrics,
            args.metrics,
            evidence_document=evidence,
        )
        marker = f"ieee80211-scalar-vector-{group_name}"
        walkthrough = REPOSITORY_ROOT / group["walkthrough"]
        if args.update:
            content = walkthrough.read_text(encoding="utf-8")
            session_id = metrics["_provenance"]["groups"][group_name][
                "session_id"
            ]
            atomic_write_text(
                walkthrough,
                update_walkthrough(
                    content,
                    marker,
                    markdown,
                    session_id,
                ),
            )
            print(f"UPDATED {walkthrough.relative_to(REPOSITORY_ROOT)}")
        else:
            print(f"{walkthrough.relative_to(REPOSITORY_ROOT)}\n")
            print(f"<!-- BEGIN GENERATED: {marker} -->")
            print(markdown.rstrip())
            print(f"<!-- END GENERATED: {marker} -->")


if __name__ == "__main__":
    main()
