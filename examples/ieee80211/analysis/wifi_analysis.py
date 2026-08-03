#!/usr/bin/env python3
"""Authoritative generation-neutral IEEE 802.11 analysis CLI."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ANALYSIS_ROOT = Path(__file__).resolve().parent
REPOSITORY_ROOT = ANALYSIS_ROOT.parents[2]
SUITES_ROOT = ANALYSIS_ROOT / "suites"
GENERATED_ROOT = ANALYSIS_ROOT / "generated"
SESSION_ID_PATTERN = re.compile(r"^\d{8}T\d{6}Z$")
PUBLICATION_RUNS = 5

sys.path.insert(0, str(ANALYSIS_ROOT))
from inet_wifi_analysis import Suite, load_suite, scenario_configuration_ini


class CliError(RuntimeError):
    """A concise user-facing command error."""


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return parsed


def session_id(value: str) -> str:
    if not SESSION_ID_PATTERN.fullmatch(value):
        raise argparse.ArgumentTypeError(
            "must have UTC format YYYYMMDDTHHMMSSZ"
        )
    return value


def new_session_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def resolve_suite_path(value: str | Path) -> Path:
    candidate = Path(value)
    if candidate.suffix != ".json" and len(candidate.parts) == 1:
        candidate = SUITES_ROOT / f"{candidate}.json"
    elif not candidate.is_absolute():
        candidate = REPOSITORY_ROOT / candidate
    if not candidate.is_file():
        raise CliError(f"Unknown suite descriptor: {value}")
    return candidate.resolve()


def relative_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPOSITORY_ROOT).as_posix()
    except ValueError:
        return str(path.resolve())


def scenario_record(
    suite_path: Path, scenario_name: str
) -> tuple[Suite, dict[str, Any]]:
    suite = load_suite(suite_path, REPOSITORY_ROOT)
    try:
        scenario = suite.scenarios[scenario_name]
    except KeyError as error:
        available = ", ".join(sorted(suite.scenarios))
        raise CliError(
            f"Unknown scenario {scenario_name!r} in suite {suite.suite!r}; "
            f"choose from: {available}"
        ) from error
    return suite, scenario


def scalar_mapping(
    suite: Suite, scenario_name: str, scenario: dict[str, Any]
) -> tuple[Path, str]:
    group = scenario.get("scalar_vector_group")
    scenario_manifest = scenario.get("scalar_vector_manifest")
    manifest_path = (
        REPOSITORY_ROOT / scenario_manifest
        if scenario_manifest is not None
        else suite.scalar_vector_manifest
    )
    if not group or manifest_path is None:
        raise CliError(
            f"Scenario {scenario_name!r} has no scalar/vector analysis mapping"
        )
    if not manifest_path.is_file():
        raise CliError(
            "Scalar/vector manifest does not exist: "
            f"{relative_path(manifest_path)}"
        )
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    if group not in document.get("groups", {}):
        raise CliError(
            f"Scenario {scenario_name!r} maps to unknown scalar/vector "
            f"group {group!r}"
        )
    return manifest_path, group


def scalar_document(
    suite: Suite, scenario_name: str, scenario: dict[str, Any]
) -> tuple[Path, str, dict[str, Any]]:
    manifest_path, group = scalar_mapping(suite, scenario_name, scenario)
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    validate_result_mapping(
        suite,
        scenario_name,
        scenario,
        document,
        group,
    )
    return (
        manifest_path,
        group,
        document,
    )


def session_directory(session: str) -> Path:
    return GENERATED_ROOT / "sessions" / session


def session_manifest_path(session: str) -> Path:
    return session_directory(session) / "session.json"


def write_json(path: Path, document: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    os.replace(temporary, path)


def filtered_scalar_manifest(
    document: dict[str, Any],
    group: str,
    runs: int,
    selected_configs: list[str] | None,
) -> dict[str, Any]:
    filtered = dict(document)
    group_document = dict(document["groups"][group])
    group_document["expected_repetitions"] = runs
    if selected_configs:
        selected = set(selected_configs)
        group_document["conditions"] = [
            condition
            for condition in group_document["conditions"]
            if condition["config"] in selected
        ]
    filtered["groups"] = {group: group_document}
    if "evidence_contracts" in document:
        filtered["evidence_contracts"] = {
            group: document["evidence_contracts"].get(group, [])
        }
    return filtered


def scalar_configs(document: dict[str, Any], group: str) -> set[str]:
    return {
        condition["config"]
        for condition in document["groups"][group]["conditions"]
    }


def ordered_scalar_configs(document: dict[str, Any], group: str) -> list[str]:
    return [
        condition["config"]
        for condition in document["groups"][group]["conditions"]
    ]


def validate_result_mapping(
    suite: Suite,
    scenario_name: str,
    scenario: dict[str, Any],
    scalar_manifest: dict[str, Any],
    scalar_group: str,
) -> None:
    """Require suite and scalar mappings to resolve each config to one example."""
    group = scalar_manifest["groups"][scalar_group]
    conditions = {
        condition["config"]: condition
        for condition in group["conditions"]
    }
    for config in scenario["configurations"]:
        condition = conditions.get(config)
        if condition is None:
            continue
        scalar_ini = REPOSITORY_ROOT / condition.get("ini", group["ini"])
        suite_ini = scenario_configuration_ini(
            suite.example_root,
            scenario,
            config,
        )
        if scalar_ini.resolve() != suite_ini.resolve():
            raise CliError(
                f"{scenario_name}/{config}: suite and scalar mappings use "
                f"different INI files: {relative_path(suite_ini)} != "
                f"{relative_path(scalar_ini)}"
            )


def check_configs(
    selected: list[str] | None,
    evidence: str,
    scenario: dict[str, Any],
    scalar_manifest: dict[str, Any] | None,
    scalar_group: str | None,
) -> None:
    if not selected:
        return
    requested = set(selected)
    errors = []
    if evidence == "both":
        unknown = sorted(requested - set(scenario["configurations"]))
        if unknown:
            errors.append("PCAP: " + ", ".join(unknown))
    if evidence in {"scalar-vector", "both"}:
        assert scalar_manifest is not None and scalar_group is not None
        unknown = sorted(
            requested - scalar_configs(scalar_manifest, scalar_group)
        )
        if unknown:
            errors.append("scalar/vector: " + ", ".join(unknown))
    if errors:
        raise CliError(
            "Configuration filter is not mapped for the selected evidence "
            "pipeline(s): " + "; ".join(errors)
        )


def run_command(command: list[str], *, matplotlib: bool = False) -> None:
    environment = None
    if matplotlib:
        environment = os.environ.copy()
        environment.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")
    subprocess.run(
        command,
        cwd=REPOSITORY_ROOT,
        env=environment,
        check=True,
    )


def inspect_command(args: argparse.Namespace) -> None:
    suite_path = resolve_suite_path(args.suite)
    suite, scenario = scenario_record(suite_path, args.scenario)
    scalar = None
    if scenario.get("scalar_vector_group"):
        manifest_path, group, document = scalar_document(
            suite, args.scenario, scenario
        )
        group_document = document["groups"][group]
        scalar = {
            "group": group,
            "manifest": relative_path(manifest_path),
            "configurations": ordered_scalar_configs(document, group),
            "default_runs": int(
                group_document.get(
                    "expected_repetitions",
                    document.get("default_repetitions", PUBLICATION_RUNS),
                )
            ),
            "measurement_window_s": {
                "start": group_document["measurement"]["start"],
                "end": group_document["measurement"]["end"],
                "notation": "[start, end)",
            },
        }
    logical_session = None
    if args.session_id is not None:
        path, logical_session = load_session(args.session_id)
        validate_session_selection(logical_session, suite, args.scenario)
        logical_session = {
            **logical_session,
            "manifest": relative_path(path),
            "publication_ready": (
                int(logical_session.get("runs", 0)) >= PUBLICATION_RUNS
            ),
        }
    print(json.dumps({
        "suite": suite.suite,
        "suite_descriptor": relative_path(suite_path),
        "scenario": args.scenario,
        "ini": relative_path(suite.example_root / scenario["ini"]),
        "pcap_configurations": scenario["configurations"],
        "configuration_inis": scenario.get("configuration_inis", {}),
        "phy_profiles": scenario["phy_profiles"],
        "scalar_vector": scalar,
        "session": logical_session,
    }, indent=2, sort_keys=True))


def load_session(session: str) -> tuple[Path, dict[str, Any]]:
    path = session_manifest_path(session)
    if not path.is_file():
        raise CliError(
            f"Logical analysis session does not exist: {relative_path(path)}"
        )
    return path, json.loads(path.read_text(encoding="utf-8"))


def validate_session_selection(
    document: dict[str, Any], suite: Suite, scenario: str
) -> None:
    if document.get("suite") != suite.suite:
        raise CliError(
            f"Session belongs to suite {document.get('suite')!r}, "
            f"not {suite.suite!r}"
        )
    if document.get("scenario") != scenario:
        raise CliError(
            f"Session belongs to scenario {document.get('scenario')!r}, "
            f"not {scenario!r}"
        )
    if document.get("evidence") not in {"scalar-vector", "both"}:
        raise CliError(
            f"Session has unsupported evidence mode "
            f"{document.get('evidence')!r}"
        )


def validate_scalar_report_session(
    logical: dict[str, Any],
    scalar_manifest: Path,
    scalar_group: str,
) -> None:
    metrics_path = scalar_manifest.parent / "metrics.json"
    if not metrics_path.is_file():
        raise CliError(
            f"Scalar/vector report does not exist: {relative_path(metrics_path)}"
        )
    metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    provenance = metrics.get("_provenance", {})
    group_provenance = provenance.get("groups", {}).get(scalar_group, {})
    if group_provenance.get("session_id") != logical["session_id"]:
        raise CliError(
            "Scalar/vector report belongs to session "
            f"{group_provenance.get('session_id')!r}, not "
            f"{logical['session_id']!r}; rerun report"
        )


def run_command_handler(args: argparse.Namespace) -> None:
    exhaustive_vectors = getattr(args, "exhaustive_vectors", False)
    suite_path = resolve_suite_path(args.suite)
    suite, scenario = scenario_record(suite_path, args.scenario)
    selected_session = args.session_id or new_session_id()
    logical_manifest = session_manifest_path(selected_session)
    if session_directory(selected_session).exists():
        raise CliError(
            f"Logical analysis session path already exists: {selected_session}"
        )

    scalar_source, scalar_group, scalar_document_value = scalar_document(
        suite, args.scenario, scenario
    )
    scalar_path = (
        session_directory(selected_session) / "scalar-vector-manifest.json"
    )
    check_configs(
        args.config,
        args.evidence,
        scenario,
        scalar_document_value,
        scalar_group,
    )
    effective_configs = args.config
    if (
        effective_configs is None
        and args.evidence == "both"
        and scalar_document_value is not None
        and scalar_group is not None
    ):
        effective_configs = ordered_scalar_configs(
            scalar_document_value, scalar_group
        )

    write_json(
        scalar_path,
        filtered_scalar_manifest(
            scalar_document_value,
            scalar_group,
            args.runs,
            effective_configs,
        ),
    )
    command = [
        sys.executable,
        str(scalar_source.parent / "run_campaign.py"),
        scalar_group,
        "--manifest",
        str(scalar_path),
        "--runs",
        str(args.runs),
        "--session-id",
        selected_session,
    ]
    for config in effective_configs or []:
        command.extend(["--config", config])
    if exhaustive_vectors:
        command.append("--exhaustive-vectors")
    if args.evidence == "both":
        command.extend(["--pcap-run", "0"])
        capture = scenario.get("capture", suite.capture)
        for pattern in capture["interface_patterns"]:
            command.extend(["--pcap-interface-pattern", pattern])
    if args.jobs is not None:
        command.extend(["--jobs", str(args.jobs)])
    run_command(command)

    if args.evidence == "both":
        command = [
            sys.executable,
            str(ANALYSIS_ROOT / "analyze_pcap.py"),
            "--suite",
            str(suite_path),
            "--index",
            "--capture-only",
            "--subdir",
            args.scenario,
            "--run",
            "0",
            "--session-id",
            selected_session,
        ]
        for config in effective_configs or []:
            command.extend(["--config", config])
        run_command(command)

    document = {
        "schema_version": 1,
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "session_id": selected_session,
        "suite": suite.suite,
        "suite_descriptor": relative_path(suite_path),
        "scenario": args.scenario,
        "evidence": args.evidence,
        "runs": args.runs,
        "run_range": {
            "start": 0,
            "end": args.runs,
            "notation": "[start, end)",
        },
        "classification": (
            "publication"
            if args.runs >= PUBLICATION_RUNS
            else "diagnostic"
        ),
        "configurations": effective_configs,
        "scalar_vector_group": scalar_group,
        "scalar_vector_manifest": relative_path(scalar_path),
        "recording": (
            scalar_document_value["groups"][scalar_group].get("recording")
            if exhaustive_vectors
            and scalar_document_value is not None
            and scalar_group is not None
            else None
        ),
        "exhaustive_vectors": exhaustive_vectors,
        "pcap_run": 0 if args.evidence == "both" else None,
        "pcap_scope": (
            "representative run 0 mechanism evidence"
            if args.evidence == "both" else None
        ),
    }
    write_json(logical_manifest, document)
    print(f"CREATED {relative_path(logical_manifest)}")
    if document["classification"] == "diagnostic":
        print(
            f"DIAGNOSTIC: {args.runs} run(s) cover [0, {args.runs}); "
            f"publication requires at least {PUBLICATION_RUNS} runs."
        )


def report_command(args: argparse.Namespace) -> None:
    suite_path = resolve_suite_path(args.suite)
    suite, scenario = scenario_record(suite_path, args.scenario)
    _, logical = load_session(args.session_id)
    validate_session_selection(logical, suite, args.scenario)
    evidence = logical["evidence"]
    configurations = logical.get("configurations") or []

    if evidence in {"scalar-vector", "both"}:
        scalar_manifest, scalar_group = scalar_mapping(
            suite, args.scenario, scenario
        )
        filtered_manifest = REPOSITORY_ROOT / logical["scalar_vector_manifest"]
        ax_analysis = scalar_manifest.parent
        run_command([
            sys.executable,
            str(ax_analysis / "summarize_results.py"),
            "--manifest",
            str(filtered_manifest),
            "--group",
            scalar_group,
            "--session-id",
            args.session_id,
        ])
        run_command([
            sys.executable,
            str(ax_analysis / "first_tranche.py"),
            scalar_group,
            "--manifest",
            str(filtered_manifest),
            "--session-id",
            args.session_id,
        ], matplotlib=True)
    if evidence == "both":
        command = [
            sys.executable,
            str(ANALYSIS_ROOT / "analyze_pcap.py"),
            "--suite",
            str(suite_path),
            "--reuse",
            "--subdir",
            args.scenario,
            "--run",
            str(logical["pcap_run"]),
            "--session-id",
            args.session_id,
        ]
        for config in configurations:
            command.extend(["--config", config])
        run_command(command, matplotlib=True)

    if evidence in {"scalar-vector", "both"}:
        run_command([
            sys.executable,
            str(ax_analysis / "evaluate_evidence.py"),
            "--manifest",
            str(filtered_manifest),
            "--session-id",
            args.session_id,
            "--group",
            scalar_group,
            "--output",
            str(session_directory(args.session_id) / "evidence-ledger.json"),
        ])


def publish_command(args: argparse.Namespace) -> None:
    if not args.update:
        raise CliError("publish requires explicit --update")
    suite_path = resolve_suite_path(args.suite)
    suite, scenario = scenario_record(suite_path, args.scenario)
    _, logical = load_session(args.session_id)
    validate_session_selection(logical, suite, args.scenario)
    if int(logical["runs"]) < PUBLICATION_RUNS:
        raise CliError(
            f"Session {args.session_id} is diagnostic ({logical['runs']} "
            f"run(s)); publication requires at least {PUBLICATION_RUNS}"
        )
    evidence = logical["evidence"]
    configurations = logical.get("configurations") or []

    if evidence in {"scalar-vector", "both"}:
        scalar_manifest, scalar_group = scalar_mapping(
            suite, args.scenario, scenario
        )
        validate_scalar_report_session(
            logical, scalar_manifest, scalar_group
        )
        run_command([
            sys.executable,
            str(scalar_manifest.parent / "render_walkthrough_results.py"),
            scalar_group,
            "--manifest",
            str(REPOSITORY_ROOT / logical["scalar_vector_manifest"]),
            "--metrics",
            str(scalar_manifest.parent / "metrics.json"),
            "--evidence-ledger",
            str(session_directory(args.session_id) / "evidence-ledger.json"),
            "--update",
        ])
    if evidence == "both":
        command = [
            sys.executable,
            str(ANALYSIS_ROOT / "analyze_pcap.py"),
            "--suite",
            str(suite_path),
            "--reuse",
            "--update-walkthrough",
            "--subdir",
            args.scenario,
            "--run",
            str(logical["pcap_run"]),
            "--session-id",
            args.session_id,
        ]
        for config in configurations:
            command.extend(["--config", config])
        run_command(command, matplotlib=True)


def init_walkthrough_command(args: argparse.Namespace) -> None:
    suite_path = resolve_suite_path(args.suite)
    suite = load_suite(suite_path, REPOSITORY_ROOT)
    example_root = REPOSITORY_ROOT / suite.example_root
    scenarios = [args.scenario] if args.scenario != "all" else list(suite.scenarios.keys())

    from inet_wifi_analysis.walkthrough import (
        SCRIPT_SESSIONS_BEGIN,
        update_script_results_session,
    )

    for scenario_name in scenarios:
        walkthrough_path = example_root / scenario_name / "walkthrough.md"
        if not walkthrough_path.is_file():
            print(f"Skipping missing walkthrough: {walkthrough_path}")
            continue
        content = walkthrough_path.read_text(encoding="utf-8")
        updated = content
        if SCRIPT_SESSIONS_BEGIN not in updated:
            updated = update_script_results_session(updated, "Scalar/vector", "NOT RUN")
            updated = update_script_results_session(updated, "PCAP", "NOT RUN")
        if updated != content:
            walkthrough_path.write_text(updated, encoding="utf-8")
            print(f"Initialized script sections in {walkthrough_path.relative_to(REPOSITORY_ROOT)}")
        else:
            print(f"Walkthrough script sections already initialized: {walkthrough_path.relative_to(REPOSITORY_ROOT)}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    inspect_parser = commands.add_parser(
        "inspect", help="show one scenario without writing files"
    )
    inspect_parser.add_argument("scenario")
    inspect_parser.add_argument("--suite", default="ax")
    inspect_parser.add_argument("--session-id", type=session_id)
    inspect_parser.set_defaults(handler=inspect_command)

    run_parser = commands.add_parser(
        "run", help="generate raw evidence for one scenario"
    )
    run_parser.add_argument("scenario")
    run_parser.add_argument("--suite", default="ax")
    run_parser.add_argument(
        "--evidence",
        choices=("scalar-vector", "both"),
        default="both",
    )
    run_parser.add_argument(
        "--runs",
        type=positive_int,
        default=PUBLICATION_RUNS,
        help="number of independent runs [0, runs); 1 is diagnostic",
    )
    run_parser.add_argument("--config", action="append")
    run_parser.add_argument("--jobs", type=positive_int)
    run_parser.add_argument(
        "--exhaustive-vectors",
        action="store_true",
        help="record the example's curated diagnostic vectors for run 0",
    )
    run_parser.add_argument("--session-id", type=session_id)
    run_parser.set_defaults(handler=run_command_handler)

    report_parser = commands.add_parser(
        "report", help="generate reports without editing walkthroughs"
    )
    report_parser.add_argument("scenario")
    report_parser.add_argument("--suite", default="ax")
    report_parser.add_argument(
        "--session-id", required=True, type=session_id
    )
    report_parser.set_defaults(handler=report_command)

    publish_parser = commands.add_parser(
        "publish", help="publish existing reports into walkthroughs"
    )
    publish_parser.add_argument("scenario")
    publish_parser.add_argument("--suite", default="ax")
    publish_parser.add_argument(
        "--session-id", required=True, type=session_id
    )
    publish_parser.add_argument(
        "--update",
        action="store_true",
        help="explicitly allow marker-bounded walkthrough updates",
    )
    publish_parser.set_defaults(handler=publish_command)

    init_parser = commands.add_parser(
        "init-walkthrough", help="initialize script marker sections in walkthrough files"
    )
    init_parser.add_argument("scenario", help="scenario name or 'all'")
    init_parser.add_argument("--suite", default="ax")
    init_parser.set_defaults(handler=init_walkthrough_command)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    try:
        args.handler(args)
    except (CliError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"error: {error}\n")


if __name__ == "__main__":
    main()

