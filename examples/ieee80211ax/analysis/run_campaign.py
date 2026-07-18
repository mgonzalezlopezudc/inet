#!/usr/bin/env python3
"""Run the configurations and repetitions declared in experiments.json."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from analysis_core import DEFAULT_MANIFEST, REPOSITORY_ROOT, load_manifest


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


@dataclass(frozen=True)
class CampaignJob:
    group: str
    config: str
    run: int
    result_dir: Path
    command: tuple[str, ...]

    @property
    def label(self) -> str:
        return f"{self.group}/{self.config} run {self.run}"


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
    command = [
        str(REPOSITORY_ROOT / "bin/inet"), "-u", "Cmdenv", "-f", str(ini),
        "-c", config, "-r", str(run),
        f"--repeat={repetitions}", f"--result-dir={result_dir}", f"--seed-set={run}",
        "--**.vector-recording=false",
        "--**.scalar-recording=false",
        "--**.heUlRandomAccessAttempt*.scalar-recording=true",
        "--**.heUlRandomAccessSuccess*.scalar-recording=true",
    ]
    command.extend(f"--**.{statistic}*.vector-recording=true" for statistic in VECTOR_STATISTICS)
    return tuple(command)


def collect_jobs(
    manifest: dict[str, Any],
    selected_group: str,
    repetitions_override: int | None = None,
    selected_configs: set[str] | None = None,
) -> list[CampaignJob]:
    group_names = sorted(manifest["groups"]) if selected_group == "all" else [selected_group]
    jobs = []
    for group_name in group_names:
        group = manifest["groups"][group_name]
        repetitions = repetitions_override or int(group["expected_repetitions"])
        for entry in group["conditions"]:
            if selected_configs and entry["config"] not in selected_configs:
                continue
            ini = REPOSITORY_ROOT / entry.get("ini", group["ini"])
            result_dir = REPOSITORY_ROOT / entry.get("result_dir", group["result_dir"])
            for run in range(repetitions):
                jobs.append(CampaignJob(
                    group=group_name,
                    config=entry["config"],
                    run=run,
                    result_dir=result_dir,
                    command=build_command(ini, result_dir, entry["config"], run, repetitions),
                ))
    return jobs


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
        "-j", "--jobs", type=positive_int, default=available_cpu_count(),
        help="maximum parallel simulations (default: all available CPUs, equivalent to nproc)",
    )
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    if args.group != "all" and args.group not in manifest["groups"]:
        parser.error(f"unknown group {args.group!r}; choose from: all, {', '.join(sorted(manifest['groups']))}")

    jobs = collect_jobs(
        manifest,
        args.group,
        repetitions_override=args.runs,
        selected_configs=set(args.config) if args.config else None,
    )
    if not run_jobs(jobs, args.jobs):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
