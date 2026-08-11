#!/usr/bin/env python3

import argparse
import json
import math
import os
import re
import resource
import statistics
import subprocess
import sys
import time
from pathlib import Path


CASE_FAMILIES = {
    "su": "Su",
    "dl": "Dl",
    "ul": "Ul",
}
CASE_STATIONS = (1, 4, 9, 37)
CASE_WIDTHS_MHZ = (20, 80, 160)
CASES = {
    f"{family}-{stations}sta-{width_mhz}mhz": f"{config_prefix}{stations}Sta{width_mhz}MHz"
    for family, config_prefix in CASE_FAMILIES.items()
    for stations in CASE_STATIONS
    for width_mhz in CASE_WIDTHS_MHZ
}

COUNTER_NAMES = (
    "mode_lookup_calls",
    "mode_entries_scanned",
    "ru_catalog_calls",
    "ru_catalog_entries_scanned",
    "ru_layout_calls",
    "he_finalizer_calls",
    "he_finalizer_candidate_calculations",
    "scheduler_calls",
    "scheduler_candidates_seen",
    "scheduler_layouts_considered",
    "packing_candidates_seen",
    "packing_length_calculations",
    "packing_ba_queries",
    "link_snapshot_projections",
    "peer_snapshot_projections",
)

CPU_USAGE_PATTERN = re.compile(
    r"Simulation CPU usage: elapsedTime = ([^,]+), numCycles = ([^,]+), numInstructions = ([^\r\n]+)"
)
RSS_PATTERN = re.compile(r"^__INET_MAX_RSS_KIB__=(\d+)$", re.MULTILINE)
PERF_PERMISSION_PATTERN = re.compile(
    r"Cannot open (?:cycles|instructions) counter: (?:Permission denied|Operation not permitted)"
)


def parse_arguments():
    parser = argparse.ArgumentParser(description="Run the IEEE 802.11 HE planning performance baseline")
    parser.add_argument("--mode", choices=("release", "profile"), default="profile")
    parser.add_argument("--case", choices=tuple(CASES), default="dl-4sta-20mhz")
    parser.add_argument("--run", type=int, default=0)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--warmups", type=int, default=3)
    parser.add_argument("--samples", type=int, default=11)
    parser.add_argument("--result-dir", type=Path, default=Path("/tmp/inet-he-planning-perf"))
    parser.add_argument("--budgets", type=Path, default=Path(__file__).with_name("budgets.json"))
    baseline_group = parser.add_mutually_exclusive_group()
    baseline_group.add_argument("--baseline", type=Path)
    baseline_group.add_argument("--write-baseline", type=Path)
    parser.add_argument("--replace-baseline", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--list-cases", action="store_true")
    return parser.parse_args()


def unavailable_counters():
    return {name: {"availability": "unavailable", "value": None} for name in COUNTER_NAMES}


def parse_number(text):
    normalized = text.strip().replace(",", "")
    if normalized.endswith("s"):
        normalized = normalized[:-1]
    return float(normalized)


def nearest_rank(values, percentile):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(percentile * len(ordered)) - 1)]


def build_simulation_command(repo_root, args, sample_dir, hardware_counters=True):
    ini_file = repo_root / "tests/speed/ieee80211heplanning/omnetpp.ini"
    return [
        str(repo_root / "bin/inet"),
        "-u", "Cmdenv",
        "-f", str(ini_file),
        "-c", CASES[args.case],
        "-r", str(args.run),
        f"--seed-set={args.seed}",
        f"--result-dir={sample_dir / 'results'}",
        "--cmdenv-express-mode=true",
        "--cmdenv-performance-display=false",
        "--cmdenv-status-frequency=1000000s",
        "--record-vector-results=false",
        "--record-scalar-results=false",
        "--record-eventlog=false",
        "--vector-recording=false",
        "--scalar-recording=false",
        "--bin-recording=false",
        "--param-recording=false",
    ] + (["--measure-cpu-usage=true"] if hardware_counters else [])


def run_sample(repo_root, args, sample_dir, kind, index):
    sample_dir.mkdir(parents=True, exist_ok=False)
    rss_file = sample_dir / "rss.txt"
    simulation_command = build_simulation_command(repo_root, args, sample_dir)
    time_format = "__INET_MAX_RSS_KIB__=%M"
    command = ["/usr/bin/time", "-f", time_format, "-o", str(rss_file), *simulation_command]
    environment = os.environ.copy()
    environment["MODE"] = args.mode
    usage_before = resource.getrusage(resource.RUSAGE_CHILDREN)
    started = time.monotonic()
    completed = subprocess.run(command, cwd=repo_root, env=environment, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    wall_time = time.monotonic() - started
    usage_after = resource.getrusage(resource.RUSAGE_CHILDREN)
    process_cpu_time = (usage_after.ru_utime - usage_before.ru_utime) + \
            (usage_after.ru_stime - usage_before.ru_stime)
    hardware_counters_available = True
    if completed.returncode != 0 and PERF_PERMISSION_PATTERN.search(completed.stderr):
        hardware_counters_available = False
        (sample_dir / "hardware-counters.stderr.log").write_text(completed.stderr, encoding="utf-8")
        simulation_command = build_simulation_command(repo_root, args, sample_dir, hardware_counters=False)
        command = ["/usr/bin/time", "-f", time_format, "-o", str(rss_file), *simulation_command]
        usage_before = resource.getrusage(resource.RUSAGE_CHILDREN)
        started = time.monotonic()
        completed = subprocess.run(command, cwd=repo_root, env=environment, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        wall_time = time.monotonic() - started
        usage_after = resource.getrusage(resource.RUSAGE_CHILDREN)
        process_cpu_time = (usage_after.ru_utime - usage_before.ru_utime) + \
                (usage_after.ru_stime - usage_before.ru_stime)
    (sample_dir / "stdout.log").write_text(completed.stdout, encoding="utf-8")
    (sample_dir / "stderr.log").write_text(completed.stderr, encoding="utf-8")
    if completed.returncode != 0:
        raise RuntimeError(
            f"benchmark {kind} {index} failed with exit code {completed.returncode}; "
            f"see {sample_dir / 'stderr.log'}"
        )
    cpu_match = CPU_USAGE_PATTERN.search(completed.stdout)
    rss_text = rss_file.read_text(encoding="utf-8")
    rss_match = RSS_PATTERN.search(rss_text)
    if rss_match is None:
        raise RuntimeError(f"peak RSS record is missing from {rss_file}")
    sample = {
        "kind": kind,
        "index": index,
        "case": args.case,
        "config": CASES[args.case],
        "run": args.run,
        "seed": args.seed,
        "mode": args.mode,
        "cwd": str(repo_root),
        "command": command,
        "simulation_result_dir": str(sample_dir / "results"),
        "cpu_time_seconds": process_cpu_time,
        "cpu_time_source": "process_rusage",
        "process_cpu_time_seconds": process_cpu_time,
        "omnet_cpu_time_seconds": parse_number(cpu_match.group(1)) if cpu_match else None,
        "cpu_cycles": int(parse_number(cpu_match.group(2))) if cpu_match else None,
        "cpu_instructions": int(parse_number(cpu_match.group(3))) if cpu_match else None,
        "hardware_counter_availability": "available" if hardware_counters_available else "permission_denied",
        "peak_rss_kib": int(rss_match.group(1)),
        "wall_time_seconds_informational": wall_time,
        "counters": unavailable_counters(),
    }
    (sample_dir / "sample.json").write_text(json.dumps(sample, indent=2) + "\n", encoding="utf-8")
    return sample


def summarize(args, repo_root, samples):
    cpu_times = [sample["cpu_time_seconds"] for sample in samples]
    instructions = [sample["cpu_instructions"] for sample in samples]
    cycles = [sample["cpu_cycles"] for sample in samples]
    rss_values = [sample["peak_rss_kib"] for sample in samples]
    cpu_median = statistics.median(cpu_times)
    cpu_p95 = nearest_rank(cpu_times, 0.95)
    available_instructions = [value for value in instructions if value is not None]
    available_cycles = [value for value in cycles if value is not None]
    instruction_metric = ({
        "availability": "available",
        "median": statistics.median(available_instructions),
        "p95": nearest_rank(available_instructions, 0.95),
    } if len(available_instructions) == len(samples) else {"availability": "unavailable"})
    cycle_metric = ({
        "availability": "available",
        "median": statistics.median(available_cycles),
        "p95": nearest_rank(available_cycles, 0.95),
    } if len(available_cycles) == len(samples) else {"availability": "unavailable"})
    return {
        "schema_version": 1,
        "case": args.case,
        "config": CASES[args.case],
        "run": args.run,
        "seed": args.seed,
        "mode": args.mode,
        "cpu_time_source": "process_rusage",
        "cwd": str(repo_root),
        "warmup_count": args.warmups,
        "sample_count": args.samples,
        "metrics": {
            "cpu_time_seconds": {"median": cpu_median, "p95": cpu_p95},
            "cpu_instructions": instruction_metric,
            "cpu_cycles": cycle_metric,
            "peak_rss_kib": {"max": max(rss_values)},
        },
        "noise": {
            "cpu_time_relative_spread": 0 if cpu_median == 0 else (cpu_p95 - cpu_median) / cpu_median,
            "instruction_relative_spread": (0 if instruction_metric.get("median") == 0 else
                    (instruction_metric["p95"] - instruction_metric["median"]) / instruction_metric["median"])
                    if instruction_metric["availability"] == "available" else None,
        },
        "counters": unavailable_counters(),
        "counter_note": "Owner-local planning counters are not instrumented in this first external baseline slice.",
    }


def compare_with_baseline(summary, baseline, budgets):
    identity_types = {
        "schema_version": int,
        "case": str,
        "config": str,
        "run": int,
        "seed": int,
        "mode": str,
        "cpu_time_source": str,
        "sample_count": int,
    }
    invalid_identity_fields = {
        field: {"current": summary.get(field), "baseline": baseline.get(field)}
        for field, expected_type in identity_types.items()
        if type(summary.get(field)) is not expected_type or type(baseline.get(field)) is not expected_type
    }
    if invalid_identity_fields:
        raise ValueError(f"baseline identity fields have invalid types: {json.dumps(invalid_identity_fields, sort_keys=True)}")
    mismatches = {
        field: {"current": summary.get(field), "baseline": baseline.get(field)}
        for field in identity_types if summary.get(field) != baseline.get(field)
    }
    if mismatches:
        raise ValueError(f"baseline identity mismatch: {json.dumps(mismatches, sort_keys=True)}")
    current = summary["metrics"]
    reference = baseline["metrics"]
    reference_values = (
        reference["cpu_time_seconds"]["median"],
        reference["cpu_time_seconds"]["p95"],
        reference["peak_rss_kib"]["max"],
    )
    if any(type(value) not in (int, float) or not math.isfinite(value) or value <= 0
            for value in reference_values):
        raise ValueError("baseline CPU and RSS metrics must be finite and positive")
    ratios = {
        "cpu_time_median_ratio": current["cpu_time_seconds"]["median"] /
                reference["cpu_time_seconds"]["median"],
        "cpu_time_p95_ratio": current["cpu_time_seconds"]["p95"] /
                reference["cpu_time_seconds"]["p95"],
        "peak_rss_ratio": current["peak_rss_kib"]["max"] / reference["peak_rss_kib"]["max"],
    }
    limits = budgets["regression_limits"]
    regressions = {name: ratio for name, ratio in ratios.items() if ratio > limits[name]}
    improvement_policy = budgets["improvement_policy"]
    median_ratio = ratios["cpu_time_median_ratio"]
    relative_gain = 1 - median_ratio
    baseline_noise = baseline.get("noise", {}).get("cpu_time_relative_spread")
    if (type(baseline_noise) not in (int, float) or
            not math.isfinite(baseline_noise) or baseline_noise < 0):
        raise ValueError("baseline CPU noise must be finite and nonnegative")
    qualified_improvement = (
        median_ratio <= improvement_policy["cpu_time_median_ratio"] and
        relative_gain > improvement_policy["minimum_baseline_noise_multiple"] * baseline_noise
    )
    return {
        "ratios": ratios,
        "limits": limits,
        "regressions": regressions,
        "qualified_improvement": qualified_improvement,
        "baseline_cpu_noise": baseline_noise,
    }


def main():
    args = parse_arguments()
    if args.list_cases:
        print(json.dumps(CASES, indent=2, sort_keys=True))
        return 0
    if args.warmups < 0 or args.samples < 1:
        raise SystemExit("--warmups must be nonnegative and --samples must be positive")
    if args.replace_baseline and not args.write_baseline:
        raise SystemExit("--replace-baseline requires --write-baseline")
    if args.write_baseline:
        baseline_path = args.write_baseline.resolve()
        if baseline_path.exists() and not args.replace_baseline:
            raise SystemExit(
                f"baseline already exists: {baseline_path}; "
                "pass --replace-baseline to replace it explicitly"
            )
    repo_root = Path(__file__).resolve().parents[3]
    preview_dir = args.result_dir / args.mode / args.case / "sample-001"
    preview_command = build_simulation_command(repo_root, args, preview_dir)
    if args.dry_run:
        print(json.dumps({"cwd": str(repo_root), "mode": args.mode, "command": preview_command}, indent=2))
        return 0

    session_name = time.strftime("%Y%m%d-%H%M%S") + f"-{os.getpid()}"
    case_dir = args.result_dir.resolve() / args.mode / args.case / session_name
    case_dir.mkdir(parents=True)
    for index in range(1, args.warmups + 1):
        run_sample(repo_root, args, case_dir / f"warmup-{index:03d}", "warmup", index)
    samples = [
        run_sample(repo_root, args, case_dir / f"sample-{index:03d}", "sample", index)
        for index in range(1, args.samples + 1)
    ]
    summary = summarize(args, repo_root, samples)
    budgets = json.loads(args.budgets.resolve().read_text(encoding="utf-8"))
    exit_code = 0
    if args.baseline:
        baseline = json.loads(args.baseline.resolve().read_text(encoding="utf-8"))
        summary["comparison"] = compare_with_baseline(summary, baseline, budgets)
        if summary["comparison"]["regressions"]:
            exit_code = 2
    summary_path = case_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    if args.write_baseline:
        baseline_path = args.write_baseline.resolve()
        baseline_path.parent.mkdir(parents=True, exist_ok=True)
        temporary_path = baseline_path.with_name(baseline_path.name + f".tmp-{os.getpid()}")
        try:
            temporary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
            if args.replace_baseline:
                os.replace(temporary_path, baseline_path)
            else:
                os.link(temporary_path, baseline_path)
                temporary_path.unlink()
        finally:
            if temporary_path.exists():
                temporary_path.unlink()
    print(json.dumps({"summary": str(summary_path), "comparison": summary.get("comparison")}, indent=2))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
