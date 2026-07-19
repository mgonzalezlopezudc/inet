#!/usr/bin/env python3
"""Run the dense-IoT 802.11ax/802.11ac comparison with Cmdenv."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path


EXAMPLE_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = EXAMPLE_DIR.parents[2]
INI = EXAMPLE_DIR / "omnetpp.ini"
RESULTS_DIR = EXAMPLE_DIR / "results"
CONFIGS = ("AxUl", "AcUl", "AxDl", "AcDl", "AxMixed", "AcMixed")
RUNS_PER_CONFIG = 15


@dataclass(frozen=True)
class Job:
    config: str
    run: int

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
            f"--result-dir={RESULTS_DIR}",
            "--cmdenv-express-mode=true",
        ]


def positive_int(text: str) -> int:
    value = int(text)
    if value < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return value


def execute(job: Job) -> tuple[Job, int, str]:
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
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
        help="run only this run number, 0..14 (repeatable)",
    )
    parser.add_argument(
        "-j", "--jobs",
        type=positive_int,
        default=1,
        help="parallel simulations (default: 1; dense runs are memory intensive)",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    configs = tuple(args.config) if args.config else CONFIGS
    runs = tuple(args.run) if args.run else tuple(range(RUNS_PER_CONFIG))
    invalid = [run for run in runs if run < 0 or run >= RUNS_PER_CONFIG]
    if invalid:
        parser.error(f"run numbers must be in 0..14, got {invalid}")
    jobs = [Job(config, run) for config in configs for run in runs]

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
