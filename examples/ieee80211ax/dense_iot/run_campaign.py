#!/usr/bin/env python3
"""Run the dense-IoT 802.11ax/802.11ac comparison with Cmdenv."""

from __future__ import annotations

import argparse
import re
import shlex
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


EXAMPLE_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = EXAMPLE_DIR.parents[2]
INI = EXAMPLE_DIR / "omnetpp.ini"
RESULTS_DIR = EXAMPLE_DIR / "results"
CONFIGS = ("AxUl", "AcUl", "AxDl", "AcDl", "AxMixed", "AcMixed")
STATION_COUNTS = (8, 16)
RUNS_PER_STATION_COUNT = 5
RUNS_PER_CONFIG = len(STATION_COUNTS) * RUNS_PER_STATION_COUNT
SESSION_ID_PATTERN = re.compile(r"^\d{8}T\d{6}Z$")


@dataclass(frozen=True)
class Job:
    config: str
    run: int
    result_dir: Path

    @property
    def label(self) -> str:
        return f"{self.config} run {self.run}"

    @property
    def command(self) -> list[str]:
        return [
            str(REPOSITORY_ROOT / "bin" / "inet"),
            "-u", "Cmdenv",
            "-f", str(INI),
            "-c", self.config,
            "-r", str(self.run),
            f"--result-dir={self.result_dir}",
            "--cmdenv-express-mode=true",
        ]


def positive_int(text: str) -> int:
    value = int(text)
    if value < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return value


def session_id(text: str) -> str:
    if not SESSION_ID_PATTERN.fullmatch(text):
        raise argparse.ArgumentTypeError("must have UTC format YYYYMMDDTHHMMSSZ")
    return text


def station_counts(text: str) -> tuple[int, ...]:
    try:
        values = tuple(int(value.strip()) for value in text.split(","))
    except ValueError:
        raise argparse.ArgumentTypeError(
            "must be a comma-separated list containing 8, 16, 32, or 64"
        ) from None
    invalid = [value for value in values if value not in STATION_COUNTS]
    if not values or invalid:
        raise argparse.ArgumentTypeError(
            "must be a comma-separated list containing 8, 16, 32, or 64"
        )
    if len(values) != len(set(values)):
        raise argparse.ArgumentTypeError("station counts must not be repeated")
    return values


def execute(job: Job) -> tuple[Job, int, str]:
    job.result_dir.mkdir(parents=True, exist_ok=True)
    print("RUN", job.label, flush=True)
    print("COMMAND", shlex.join(job.command), flush=True)
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
            return job, completed.returncode, output.read()
    return job, 0, ""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        action="append",
        choices=CONFIGS,
        help="run only this configuration (repeatable)",
    )
    parser.add_argument(
        "--run",
        action="append",
        type=int,
        help=f"run only this run number, 0..{RUNS_PER_CONFIG - 1} (repeatable)",
    )
    parser.add_argument(
        "--station-counts", "--station-count", "--stations",
        type=station_counts,
        help="comma-separated station counts: 8, 16, 32, and/or 64 (default: all)",
    )
    parser.add_argument(
        "--runs-per-station-count", "--runs",
        type=int,
        choices=range(1, RUNS_PER_STATION_COUNT + 1),
        help="number of runs per station count, 1..5 (default: 5)",
    )
    parser.add_argument(
        "-j", "--jobs",
        type=positive_int,
        default=1,
        help="parallel simulations (default: 1; dense runs are memory intensive)",
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument(
        "--session-id",
        type=session_id,
        help="UTC result-set ID (default: current time as YYYYMMDDTHHMMSSZ)",
    )
    args = parser.parse_args()

    configs = tuple(args.config) if args.config else CONFIGS
    if args.run and (args.station_counts or args.runs_per_station_count):
        parser.error(
            "--run cannot be combined with --station-counts or "
            "--runs-per-station-count"
        )
    if args.run:
        runs = tuple(args.run)
    else:
        selected_station_counts = args.station_counts or STATION_COUNTS
        run_count = args.runs_per_station_count or RUNS_PER_STATION_COUNT
        runs = tuple(
            STATION_COUNTS.index(station_count) * RUNS_PER_STATION_COUNT + repetition
            for station_count in selected_station_counts
            for repetition in range(run_count)
        )
    invalid = [run for run in runs if run < 0 or run >= RUNS_PER_CONFIG]
    if invalid:
        parser.error(
            f"run numbers must be in 0..{RUNS_PER_CONFIG - 1}, got {invalid}"
        )
    campaign_session_id = (
        args.session_id
        or datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    )
    print(f"Campaign session: {campaign_session_id}", flush=True)
    jobs = [
        Job(
            config,
            run,
            RESULTS_DIR / campaign_session_id / config,
        )
        for config in configs
        for run in runs
    ]

    if args.dry_run:
        for job in jobs:
            print(shlex.join(job.command))
        return

    failures: list[tuple[Job, int, str]] = []
    with ThreadPoolExecutor(max_workers=min(args.jobs, len(jobs))) as executor:
        futures = {executor.submit(execute, job): job for job in jobs}
        for count, future in enumerate(as_completed(futures), 1):
            result = future.result()
            job, returncode, _ = result
            print(
                f"[{count}/{len(jobs)}] "
                f"{'DONE' if returncode == 0 else f'FAILED ({returncode})'} "
                f"{job.label}",
                flush=True,
            )
            if returncode:
                failures.append(result)

    for job, returncode, output in failures:
        print(f"\n===== {job.label}: exit {returncode} =====", file=sys.stderr)
        print(output.rstrip(), file=sys.stderr)
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
