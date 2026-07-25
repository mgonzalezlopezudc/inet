#!/usr/bin/env python3
"""Validate the structure of an INET IEEE 802.11 walkthrough."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REQUIRED_HEADINGS = (
    "learning objectives and feature primer",
    "scenario description",
    "standards and inet model boundary",
    "evidence status",
    "configuration matrix",
    "expected invariants and diagnostic map",
    "reproduction",
    "scalar and vector analysis",
    "pcap statistics",
    "frame exchange analysis",
    "cross-layer findings and verdict",
    "limitations and inconclusive claims",
)

PLACEHOLDER_PATTERNS = (
    re.compile(r"\bTODO\b", re.IGNORECASE),
    re.compile(r"<(?:IEEE|One |explain|identify|relate|reproduce|Describe|Name |"
               r"Include|Treat|failed|normative|signals|legacy|requirements|owner|"
               r"feature|representative|outcome|result|field|runs?|scope|"
               r"control|treatment|disabled|enabled|matched|counterfactual|"
               r"specific|artifact|observable|module|focused|example|"
               r"Configuration|configuration|session|file|narrow|metric|"
               r"node|format|precision|display|count|types|number|seconds|"
               r"source|destination|frame|PPDU|protocol|claim|effective|"
               r"configs|query|window|hash|relative)[^>]*>",
               re.IGNORECASE),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Check an INET IEEE 802.11 walkthrough contract."
    )
    parser.add_argument("walkthrough", type=Path)
    parser.add_argument(
        "--allow-placeholders",
        action="store_true",
        help="Allow template placeholders while still checking structure.",
    )
    parser.add_argument(
        "--require-analysis-visuals",
        action="store_true",
        help=(
            "Require a compact table and a plot or explicit no-plot rationale "
            "inside both analysis sections; otherwise report migration warnings"
        ),
    )
    return parser.parse_args()


def normalize_heading(line: str) -> str | None:
    match = re.match(r"^#{2,6}\s+(.+?)\s*$", line)
    if match is None:
        return None
    heading = re.sub(r"[`*_]", "", match.group(1))
    return re.sub(r"\s+", " ", heading).strip().lower()


def section_body(text: str, label: str) -> str | None:
    headings = list(re.finditer(r"^##\s+(.+?)\s*$", text, re.MULTILINE))
    selected_index = next(
        (
            index
            for index, heading in enumerate(headings)
            if normalize_heading(f"## {heading.group(1)}") == label.lower()
        ),
        None,
    )
    if selected_index is None:
        return None
    heading = headings[selected_index]
    end = (
        headings[selected_index + 1].start()
        if selected_index + 1 < len(headings)
        else len(text)
    )
    return text[heading.end():end]


def has_markdown_table(text: str) -> bool:
    return bool(re.search(
        r"^\|.*\|\s*\n\|(?:\s*:?-{3,}:?\s*\|){2,}\s*$",
        text,
        re.MULTILINE,
    ))


def has_plot_or_rationale(text: str) -> bool:
    if re.search(r"!\[[^\]]+\]\([^)]+\.(?:png|svg|pdf)\)", text, re.IGNORECASE):
        return True
    return bool(
        re.search(
            r"\b(?:no plot|plot omitted|no figure|figure omitted)\s*:\s*\S",
            text,
            re.IGNORECASE,
        )
        or re.search(
            r"\b(?:no plot|plot omitted|no figure|figure omitted)\b"
            r"(?:(?!\n\n).)*(?:because|reason|not useful|not meaningful|"
            r"would not|cannot)",
            text,
            re.IGNORECASE | re.DOTALL,
        )
    )


def main() -> int:
    args = parse_args()
    path = args.walkthrough
    if not path.is_file():
        print(f"ERROR: walkthrough does not exist: {path}")
        return 2

    text = path.read_text(encoding="utf-8")
    headings = [
        heading
        for line in text.splitlines()
        if (heading := normalize_heading(line)) is not None
    ]

    errors: list[str] = []
    warnings: list[str] = []

    for label in REQUIRED_HEADINGS:
        if label not in headings:
            errors.append(f"missing required section: {label}")

    if not args.allow_placeholders:
        for pattern in PLACEHOLDER_PATTERNS:
            if pattern.search(text):
                errors.append(
                    f"unresolved placeholder matched: {pattern.pattern}"
                )

    if not re.search(r"`(?:PASS|FAIL|INCONCLUSIVE|NOT RUN)`", text):
        errors.append("no evidence status value found")
    if not re.search(r"```(?:sh|bash)\s", text):
        errors.append("no reproducible shell command block found")
    if not re.search(r"\.(?:sca|vec)\b", text, re.IGNORECASE):
        errors.append("no .sca or .vec artifact named")
    if not re.search(r"\.(?:pcap|pcapng)\b", text, re.IGNORECASE):
        errors.append("no PCAP artifact named")
    if not re.search(r"\btshark\b", text, re.IGNORECASE):
        errors.append("no TShark command or reference found")
    if not re.search(r"^\|.+\|\s*$", text, re.MULTILINE):
        errors.append("no Markdown evidence table found")
    if re.search(r"(?:\]\(|\s)/home/", text):
        errors.append("absolute /home path found; use relative paths")

    generated_starts = len(
        re.findall(r"<!--\s*BEGIN GENERATED:", text, re.IGNORECASE)
    )
    generated_ends = len(
        re.findall(r"<!--\s*END GENERATED:", text, re.IGNORECASE)
    )
    if generated_starts != generated_ends:
        errors.append(
            "generated-section markers are unbalanced: "
            f"{generated_starts} BEGIN, {generated_ends} END"
        )

    for label in ("Scalar and vector analysis", "PCAP statistics"):
        body = section_body(text, label)
        if body is None:
            continue
        section_findings: list[str] = []
        if not has_markdown_table(body):
            section_findings.append(
                f"{label}: no compact Markdown table found in section"
            )
        if not has_plot_or_rationale(body):
            section_findings.append(
                f"{label}: include a plot in the section or an explicit "
                "no-plot rationale"
            )
        (errors if args.require_analysis_visuals else warnings).extend(
            section_findings
        )
        if re.search(
            r"!\[[^\]]+\]\([^)]+\.png\)", body, re.IGNORECASE
        ) and not re.search(r"\.png\.json\b", body, re.IGNORECASE):
            warnings.append(
                f"{label}: PNG figure has no provenance sidecar reference"
            )

    if not re.search(r"\b(?:run|runs)\b", text, re.IGNORECASE):
        warnings.append("run coverage is not stated")
    if not re.search(r"\bseed", text, re.IGNORECASE):
        warnings.append("seed policy is not stated")
    if not re.search(r"\b(?:warm-?up|measurement window|time window)\b",
                     text, re.IGNORECASE):
        warnings.append("warm-up or measurement-window policy is not stated")
    if not re.search(r"\b(?:capture point|observation point|captures? observe|"
                     r"captured at)\b",
                     text, re.IGNORECASE):
        warnings.append("PCAP capture/observation point is not stated")
    if not re.search(r"\b(?:direct observations?|derived measurements?|"
                     r"inferences?)\b",
                     text, re.IGNORECASE):
        warnings.append("evidence basis is not labeled")

    for message in errors:
        print(f"ERROR: {message}")
    for message in warnings:
        print(f"WARNING: {message}")

    if errors:
        print(
            f"FAILED: {path} has {len(errors)} error(s) "
            f"and {len(warnings)} warning(s)."
        )
        return 1

    print(f"PASS: {path} has 0 errors and {len(warnings)} warning(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
