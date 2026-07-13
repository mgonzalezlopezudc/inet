#!/usr/bin/env python3
"""Run exactly the configurations and repetitions declared in experiments.json."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from analysis_core import DEFAULT_MANIFEST, REPOSITORY_ROOT, load_manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("group", choices=["all"] + sorted(load_manifest()["groups"]))
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--runs", type=int, help="override repetition count for a diagnostic campaign")
    parser.add_argument("--config", action="append", help="run only the named configuration")
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    groups = sorted(manifest["groups"]) if args.group == "all" else [args.group]
    for group_name in groups:
        group = manifest["groups"][group_name]
        repetitions = args.runs or int(group["expected_repetitions"])
        for entry in group["conditions"]:
            if args.config and entry["config"] not in args.config:
                continue
            ini = REPOSITORY_ROOT / entry.get("ini", group["ini"])
            result_dir = REPOSITORY_ROOT / entry.get("result_dir", group["result_dir"])
            result_dir.mkdir(parents=True, exist_ok=True)
            for run in range(repetitions):
                command = [
                    str(REPOSITORY_ROOT / "bin/inet"), "-u", "Cmdenv", "-f", str(ini),
                    "-c", entry["config"], "-r", str(run),
                    f"--repeat={repetitions}", f"--result-dir={result_dir}", f"--seed-set={run}",
                    "--**.vector-recording=false",
                    "--**.scalar-recording=false",
                    "--**.heUlRandomAccessAttempt*.scalar-recording=true",
                    "--**.heUlRandomAccessSuccess*.scalar-recording=true",
                ]
                for statistic in (
                    "packetReceived", "endToEndDelay", "packetSentToPeer",
                    "acknowledgmentFrameType", "acknowledgmentAirtime",
                    "radioMode", "powerConsumption", "transmissionState",
                    "heRateSelectedMcs", "heRateSelectedNss",
                    "heRateSuccessProbability", "heRateTxSuccess", "heRateRetryCount",
                    "heUlBufferStatusReportedBytes", "heUlBufferStatusScheduledBytes",
                    "heRuToneOffset", "heRuToneSize", "heStaId",
                    "hePuncturedSubchannelMask", "heSpatialStreams", "heStreamStartIndex",
                ):
                    command.append(f"--**.{statistic}*.vector-recording=true")
                print("RUN", " ".join(command), flush=True)
                subprocess.run(command, cwd=REPOSITORY_ROOT, check=True)


if __name__ == "__main__":
    main()
