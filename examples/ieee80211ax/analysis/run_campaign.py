#!/usr/bin/env python3
"""Run the configurations and repetitions declared in experiments.json."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from analysis_core import DEFAULT_MANIFEST, REPOSITORY_ROOT, load_manifest

GENERAL_ANALYSIS_ROOT = Path(__file__).resolve().parents[2] / "ieee80211" / "analysis"
if str(GENERAL_ANALYSIS_ROOT) not in sys.path:
    sys.path.insert(0, str(GENERAL_ANALYSIS_ROOT))

from inet_wifi_analysis import (
    CampaignJob,
    build_cmdenv_command,
    collect_campaign_jobs,
)

VECTOR_STATISTICS = (
    "packetReceived", "endToEndDelay", "packetSentToPeer", "packetDropIncorrectlyReceived",
    "acknowledgmentFrameType", "acknowledgmentAirtime",
    "radioMode", "powerConsumption", "transmissionState",
    "heRateSelectedMcs", "heRateSelectedNss",
    "heRateSuccessProbability", "heRateTxSuccess", "heRateRetryCount",
    "heUlBufferStatusReportedBytes", "heUlBufferStatusScheduledBytes",
    "heRuToneOffset", "heRuToneSize", "heStaId",
    "hePuncturedSubchannelMask", "heSpatialStreams", "heStreamStartIndex",
    "heScheduledPsduBytes", "heUserPpduDuration",
    "heSpatialReuseBssType", "heSpatialReuseReceivedBssColor", "heSpatialReuseLocalBssColor",
    "heSpatialReuseEligible", "heSpatialReuseIgnoredPpdu", "heSpatialReuseObssPdThreshold",
    "heSpatialReuseTransmitPowerLimit",
    "heSpatialReuseReason",
)
SESSION_ID_PATTERN = re.compile(r"^\d{8}T\d{6}Z$")


@dataclass(frozen=True)
class JobResult:
    job: CampaignJob
    returncode: int
    output: str


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return parsed


def session_id(value: str) -> str:
    if not SESSION_ID_PATTERN.fullmatch(value):
        raise argparse.ArgumentTypeError("must have UTC format YYYYMMDDTHHMMSSZ")
    return value


def new_session_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def available_cpu_count() -> int:
    """Return the CPUs available to this process, respecting affinity limits."""
    try:
        return len(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        return os.cpu_count() or 1


def build_command(
    ini: Path,
    result_dir: Path,
    config: str,
    run: int,
    repetitions: int,
) -> tuple[str, ...]:
    return build_cmdenv_command(
        REPOSITORY_ROOT,
        ini,
        result_dir,
        config,
        run,
        repetitions,
        VECTOR_STATISTICS,
        ("heUlRandomAccessAttempt", "heUlRandomAccessSuccess"),
    )


def collect_jobs(
    manifest: dict[str, Any],
    selected_group: str,
    repetitions_override: int | None = None,
    selected_configs: set[str] | None = None,
    campaign_session_id: str | None = None,
) -> list[CampaignJob]:
    return collect_campaign_jobs(
        manifest,
        selected_group,
        REPOSITORY_ROOT,
        campaign_session_id or new_session_id(),
        VECTOR_STATISTICS,
        ("heUlRandomAccessAttempt", "heUlRandomAccessSuccess"),
        repetitions_override,
        selected_configs,
    )


def execute_job(job: CampaignJob) -> JobResult:
    print("RUN", job.label, "::", " ".join(job.command), flush=True)
    job.result_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as output:
        completed = subprocess.run(
            job.command,
            cwd=REPOSITORY_ROOT,
            stdout=output,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if completed.returncode:
            output.seek(0)
            return JobResult(job, completed.returncode, output.read())
    return JobResult(job, 0, "")


def run_jobs(jobs: list[CampaignJob], jobs_limit: int) -> bool:
    if not jobs:
        print("No simulation runs matched the selection.", file=sys.stderr)
        return False

    workers = min(jobs_limit, len(jobs))
    print(f"Executing {len(jobs)} simulation runs with {workers} parallel worker(s).", flush=True)
    failures = []
    with ThreadPoolExecutor(max_workers=workers, thread_name_prefix="simulation") as executor:
        futures = {executor.submit(execute_job, job): job for job in jobs}
        for completed_count, future in enumerate(as_completed(futures), 1):
            result = future.result()
            status = "DONE" if result.returncode == 0 else f"FAILED ({result.returncode})"
            print(f"[{completed_count}/{len(jobs)}] {status} {result.job.label}", flush=True)
            if result.returncode:
                failures.append(result)

    for failure in failures:
        print(f"\n===== {failure.job.label}: exit {failure.returncode} =====", file=sys.stderr)
        print(failure.output.rstrip(), file=sys.stderr)
    return not failures


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("group", help="manifest group name, or 'all'")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--runs", type=positive_int, help="override repetition count for a diagnostic campaign")
    parser.add_argument("--config", action="append", help="run only the named configuration")
    parser.add_argument(
        "--session-id",
        type=session_id,
        help="UTC result-set ID (default: current time as YYYYMMDDTHHMMSSZ)",
    )
    parser.add_argument(
        "-j", "--jobs", type=positive_int, default=available_cpu_count(),
        help="maximum parallel simulations (default: all available CPUs, equivalent to nproc)",
    )
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    if args.group != "all" and args.group not in manifest["groups"]:
        parser.error(f"unknown group {args.group!r}; choose from: all, {', '.join(sorted(manifest['groups']))}")

    campaign_session_id = args.session_id or new_session_id()
    print(f"Campaign session: {campaign_session_id}", flush=True)
    jobs = collect_jobs(
        manifest,
        args.group,
        repetitions_override=args.runs,
        selected_configs=set(args.config) if args.config else None,
        campaign_session_id=campaign_session_id,
    )
    if not run_jobs(jobs, args.jobs):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
