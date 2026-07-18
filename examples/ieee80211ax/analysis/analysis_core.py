#!/usr/bin/env python3
"""Strict result loading and aggregation for the IEEE 802.11ax analyses."""

from __future__ import annotations

import hashlib
import json
import math
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

import numpy as np
import pandas as pd
from omnetpp.scave import results
from scipy.stats import t


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
ANALYSIS_DIR = Path(__file__).resolve().parent
DEFAULT_MANIFEST = ANALYSIS_DIR / "experiments.json"

QUERY_OPTIONS = {
    "include_attrs": True,
    "include_runattrs": True,
    "include_itervars": True,
    "include_config_entries": True,
}


def load_manifest(path: Path = DEFAULT_MANIFEST) -> dict[str, Any]:
    with path.open(encoding="utf-8") as stream:
        manifest = json.load(stream)
    if manifest.get("schema_version") != 1:
        raise RuntimeError(f"Unsupported analysis manifest schema in {path}")
    validate_evidence_contracts(manifest)
    return manifest


def validate_evidence_contracts(manifest: dict[str, Any]) -> None:
    groups = set(manifest.get("groups", {}))
    contracts = manifest.get("evidence_contracts")
    if not isinstance(contracts, dict) or set(contracts) != groups:
        raise RuntimeError("Evidence contracts must exist for every analysis group and no others")
    for group_name, requirements in contracts.items():
        if not isinstance(requirements, list) or not requirements:
            raise RuntimeError(f"{group_name}: evidence contract must contain requirements")
        for requirement in requirements:
            if requirement.get("kind") not in {"normative", "model", "metric"}:
                raise RuntimeError(f"{group_name}: invalid evidence-contract kind")
            if not requirement.get("requirement"):
                raise RuntimeError(f"{group_name}: missing evidence-contract requirement")
            if not isinstance(requirement.get("results"), list) or not requirement["results"]:
                raise RuntimeError(f"{group_name}: evidence-contract result list is empty")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git_revision() -> str | None:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=REPOSITORY_ROOT,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


@dataclass(frozen=True)
class MeasurementWindow:
    start: float
    end: float

    def __post_init__(self) -> None:
        if not math.isfinite(self.start) or not math.isfinite(self.end):
            raise ValueError("Measurement-window endpoints must be finite")
        if self.end <= self.start:
            raise ValueError("Measurement-window end must be after its start")

    @property
    def duration(self) -> float:
        return self.end - self.start


@dataclass(frozen=True)
class ResultFiles:
    run_number: int
    scalar: Path
    vector: Path

    @property
    def paths(self) -> tuple[Path, Path]:
        return self.scalar, self.vector


class Condition:
    """One experimental condition containing independent simulation runs."""

    def __init__(
        self,
        *,
        group: str,
        label: str,
        config: str,
        ini: Path,
        result_dir: Path,
        expected_repetitions: int,
        measurement: MeasurementWindow,
        condition_metadata: dict[str, Any] | None = None,
    ) -> None:
        self.group = group
        self.label = label
        self.config = config
        self.ini = ini
        self.result_dir = result_dir
        self.expected_repetitions = expected_repetitions
        self.measurement = measurement
        self.condition_metadata = condition_metadata or {}
        self.result_files = self._discover_result_files()

    @classmethod
    def from_manifest(
        cls,
        group_name: str,
        group: dict[str, Any],
        entry: dict[str, Any],
    ) -> "Condition":
        root = REPOSITORY_ROOT
        window_data = entry.get("measurement", group["measurement"])
        return cls(
            group=group_name,
            label=entry["label"],
            config=entry["config"],
            ini=root / group["ini"],
            result_dir=root / entry.get("result_dir", group["result_dir"]),
            expected_repetitions=int(entry.get(
                "expected_repetitions", group["expected_repetitions"]
            )),
            measurement=MeasurementWindow(
                float(window_data["start"]), float(window_data["end"])
            ),
            condition_metadata=entry.get("metadata", {}),
        )

    def _discover_result_files(self) -> tuple[ResultFiles, ...]:
        if not self.result_dir.is_dir():
            raise FileNotFoundError(self.result_dir)
        pattern = re.compile(
            rf"^{re.escape(self.config)}-#(?P<run>\d+)\.(?P<extension>sca|vec)$"
        )
        discovered: dict[int, dict[str, Path]] = {}
        for path in self.result_dir.iterdir():
            match = pattern.match(path.name)
            if match:
                run = int(match.group("run"))
                discovered.setdefault(run, {})[match.group("extension")] = path
        if not discovered:
            raise FileNotFoundError(
                f"No result files for {self.config} in {self.result_dir}"
            )
        pairs: list[ResultFiles] = []
        for run, files in sorted(discovered.items()):
            missing = {"sca", "vec"} - files.keys()
            if missing:
                raise RuntimeError(
                    f"{self.config} run {run} is missing {sorted(missing)}"
                )
            pairs.append(ResultFiles(run, files["sca"], files["vec"]))
        if len(pairs) != self.expected_repetitions:
            raise RuntimeError(
                f"{self.config}: expected {self.expected_repetitions} runs, "
                f"found {len(pairs)} ({[pair.run_number for pair in pairs]})"
            )
        return tuple(pairs)

    @property
    def paths(self) -> list[str]:
        return [str(path) for pair in self.result_files for path in pair.paths]

    def _read(self, expression: str) -> pd.DataFrame:
        return results.read_result_files(self.paths, filter_expression=expression)

    def vectors(
        self,
        name: str,
        *,
        module: str = "**",
        expected_unit: str | None = None,
        required: bool = True,
    ) -> pd.DataFrame:
        expression = f'module =~ "{module}" AND name =~ "{name}"'
        frame = results.get_vectors(
            self._read(expression),
            omit_empty_vectors=True,
            **QUERY_OPTIONS,
        )
        self._validate_common(frame, name, required)
        if frame.empty:
            return frame
        required_columns = {"vectime", "vecvalue"}
        missing = required_columns - set(frame.columns)
        if missing:
            raise RuntimeError(f"{self.config}/{name}: missing {sorted(missing)}")
        for index, row in frame.iterrows():
            times = np.asarray(row.vectime, dtype=float)
            values = np.asarray(row.vecvalue, dtype=float)
            if len(times) != len(values):
                raise RuntimeError(f"{self.config}/{name}: row {index} is unaligned")
            if not len(times):
                raise RuntimeError(f"{self.config}/{name}: row {index} is empty")
            if np.any(np.diff(times) < 0):
                raise RuntimeError(
                    f"{self.config}/{name}: row {index} has nonmonotonic time"
                )
        self._validate_unit(frame, name, expected_unit)
        self._print_matches("vector", name, expression, frame)
        return frame

    def scalars(
        self,
        name: str,
        *,
        module: str = "**",
        expected_unit: str | None = None,
        required: bool = True,
    ) -> pd.DataFrame:
        expression = f'module =~ "{module}" AND name =~ "{name}"'
        frame = results.get_scalars(self._read(expression), **QUERY_OPTIONS)
        self._validate_common(frame, name, required)
        if frame.empty:
            return frame
        frame = frame.copy()
        frame["value"] = pd.to_numeric(frame["value"], errors="raise")
        self._validate_unit(frame, name, expected_unit)
        self._print_matches("scalar", name, expression, frame)
        return frame

    def _validate_common(
        self, frame: pd.DataFrame, name: str, required: bool
    ) -> None:
        if frame.empty:
            if required:
                raise RuntimeError(f"{self.config}: no results matched {name!r}")
            return
        required_columns = {"runID", "module", "name", "configname", "runnumber"}
        missing = required_columns - set(frame.columns)
        if missing:
            raise RuntimeError(f"{self.config}/{name}: missing {sorted(missing)}")
        configurations = set(frame["configname"].dropna().astype(str))
        if configurations != {self.config}:
            raise RuntimeError(
                f"{self.config}/{name}: matched configurations {sorted(configurations)}"
            )
        run_numbers = set(pd.to_numeric(frame["runnumber"], errors="raise").astype(int))
        expected = {pair.run_number for pair in self.result_files}
        if run_numbers != expected:
            raise RuntimeError(
                f"{self.config}/{name}: matched runs {sorted(run_numbers)}, "
                f"expected {sorted(expected)}"
            )

    @staticmethod
    def _validate_unit(
        frame: pd.DataFrame, name: str, expected_unit: str | None
    ) -> None:
        if expected_unit is None:
            return
        if "unit" not in frame:
            raise RuntimeError(f"{name}: unit metadata is missing")
        units = set(frame["unit"].dropna().astype(str))
        if units != {expected_unit}:
            raise RuntimeError(
                f"{name}: expected unit {expected_unit!r}, found {sorted(units)}"
            )

    def _print_matches(
        self, kind: str, name: str, expression: str, frame: pd.DataFrame
    ) -> None:
        matches = frame[["runID", "module", "name"]].drop_duplicates()
        print(
            f"MATCH {self.group}/{self.config} {kind} {name}: "
            f"{len(matches)} rows; filter={expression}"
        )

    def provenance(self) -> dict[str, Any]:
        return {
            "group": self.group,
            "label": self.label,
            "configuration": self.config,
            "ini": str(self.ini.relative_to(REPOSITORY_ROOT)),
            "measurement": {
                "start_s": self.measurement.start,
                "end_s": self.measurement.end,
            },
            "metadata": self.condition_metadata,
            "result_files": [
                {
                    "path": str(path.relative_to(REPOSITORY_ROOT)),
                    "sha256": sha256(path),
                }
                for pair in self.result_files
                for path in pair.paths
            ],
        }


def conditions_for_group(
    manifest: dict[str, Any], group_name: str
) -> list[Condition]:
    try:
        group = manifest["groups"][group_name]
    except KeyError as error:
        raise KeyError(f"Unknown analysis group {group_name!r}") from error
    return [
        Condition.from_manifest(group_name, group, entry)
        for entry in group["conditions"]
    ]


def crop_vector(
    times: Iterable[float], values: Iterable[float], window: MeasurementWindow
) -> tuple[np.ndarray, np.ndarray]:
    times_array = np.asarray(times, dtype=float)
    values_array = np.asarray(values, dtype=float)
    if len(times_array) != len(values_array):
        raise ValueError("Vector timestamps and values are unaligned")
    selected = (times_array >= window.start) & (times_array <= window.end)
    return times_array[selected], values_array[selected]


def app_sink_vectors(condition: Condition, name: str) -> pd.DataFrame:
    # OMNeT++'s packetBytes recorder yields bytes but currently stores an empty
    # unit attribute; the exact recorder name is therefore the unit contract.
    frame = condition.vectors(name, module="**.app[*]")
    sink_pattern = condition.condition_metadata.get(
        "sink_module_regex", r"\.(?:host|sta\d*|sta)\[\d+\]\.app\["
    )
    frame = frame[frame.module.str.contains(sink_pattern, regex=True)].copy()
    if frame.empty:
        raise RuntimeError(
            f"{condition.config}: no application sinks matched {sink_pattern!r}"
        )
    return frame


def per_run_goodput(condition: Condition) -> pd.DataFrame:
    nodes = per_run_node_goodput(condition)
    result = (
        nodes.groupby(["runID", "runnumber"], as_index=False)
        .agg(goodput_bps=("goodput_bps", "sum"), delivered_bytes=("delivered_bytes", "sum"))
    )
    if len(result) != condition.expected_repetitions:
        raise RuntimeError(
            f"{condition.config}: expected one goodput observation per run"
        )
    return result


def per_run_node_goodput(condition: Condition) -> pd.DataFrame:
    frame = app_sink_vectors(condition, "packetReceived:vector(packetBytes)")
    records: list[dict[str, Any]] = []
    for _, row in frame.iterrows():
        _, values = crop_vector(row.vectime, row.vecvalue, condition.measurement)
        delivered_bytes = float(np.sum(values))
        records.append({
            "runID": row.runID,
            "runnumber": int(row.runnumber),
            "module": row.module,
            "goodput_bps": delivered_bytes * 8 / condition.measurement.duration,
            "delivered_bytes": delivered_bytes,
        })
    return pd.DataFrame.from_records(records)


def per_run_scalar_sum(
    condition: Condition,
    name: str,
    *,
    module: str = "**",
    expected_unit: str | None = None,
) -> pd.DataFrame:
    frame = condition.scalars(name, module=module, expected_unit=expected_unit)
    return (
        frame.groupby("runID", as_index=False)["value"]
        .sum()
        .rename(columns={"value": "sum"})
    )


def per_run_delay_percentile(
    condition: Condition, percentile: float
) -> pd.DataFrame:
    frame = condition.vectors(
        "endToEndDelay:vector", module="**.app[*]", expected_unit="s"
    )
    frame = frame[
        frame.module.str.contains(r"\.(?:host|sta\d*|sta)\[\d+\]\.app\[", regex=True)
    ]
    records: list[dict[str, Any]] = []
    for run_id, rows in frame.groupby("runID"):
        samples = []
        for _, row in rows.iterrows():
            _, values = crop_vector(row.vectime, row.vecvalue, condition.measurement)
            samples.append(values)
        values = np.concatenate(samples) if samples else np.array([])
        if not len(values):
            raise RuntimeError(f"{condition.config}/{run_id}: no delay samples")
        records.append({
            "runID": run_id,
            "delay_s": float(np.percentile(values, percentile)),
        })
    return pd.DataFrame.from_records(records)


def summarize_ci95(values: Iterable[float]) -> dict[str, float | int]:
    array = np.asarray(list(values), dtype=float)
    if not len(array) or np.any(~np.isfinite(array)):
        raise ValueError("Summary values must be finite and nonempty")
    count = len(array)
    mean = float(np.mean(array))
    if count == 1:
        return {"mean": mean, "ci95": math.nan, "count": count}
    standard_error = float(np.std(array, ddof=1) / math.sqrt(count))
    return {
        "mean": mean,
        "ci95": float(t.ppf(0.975, count - 1) * standard_error),
        "count": count,
    }


def jain(values: Iterable[float]) -> float:
    array = np.asarray(list(values), dtype=float)
    denominator = len(array) * float(np.square(array).sum())
    return float(array.sum() ** 2 / denominator) if denominator else math.nan


def time_weighted_integral(
    times: Iterable[float], values: Iterable[float], window: MeasurementWindow
) -> float:
    times_array = np.asarray(times, dtype=float)
    values_array = np.asarray(values, dtype=float)
    if len(times_array) != len(values_array) or not len(times_array):
        raise ValueError("Piecewise-constant vector is empty or unaligned")
    if np.any(np.diff(times_array) < 0):
        raise ValueError("Piecewise-constant timestamps are nonmonotonic")
    breakpoints = np.concatenate((
        [window.start],
        times_array[(times_array > window.start) & (times_array < window.end)],
        [window.end],
    ))
    indices = np.searchsorted(times_array, breakpoints[:-1], side="right") - 1
    if np.any(indices < 0):
        raise ValueError("Vector has no value at measurement-window start")
    return float(np.sum(values_array[indices] * np.diff(breakpoints)))


def validate_disjoint_streams(
    station_ids: Iterable[int], starts: Iterable[int], counts: Iterable[int]
) -> None:
    intervals = sorted(
        (int(start), int(start) + int(count), int(station))
        for station, start, count in zip(station_ids, starts, counts)
    )
    for left, right in zip(intervals, intervals[1:]):
        if left[1] > right[0]:
            raise RuntimeError(
                f"MU-MIMO stream overlap: STA {left[2]} {left[:2]} and "
                f"STA {right[2]} {right[:2]}"
            )


def validate_unpunctured_ru(
    tone_offset: int,
    tone_size: int,
    punctured_ranges: Iterable[tuple[int, int]],
) -> None:
    ru_start = int(tone_offset)
    ru_end = ru_start + int(tone_size)
    for punctured_start, punctured_end in punctured_ranges:
        if ru_start < punctured_end and punctured_start < ru_end:
            raise RuntimeError(
                f"RU [{ru_start}, {ru_end}) overlaps punctured range "
                f"[{punctured_start}, {punctured_end})"
            )


def write_provenance(
    output: Path,
    *,
    conditions: list[Condition],
    result_filters: list[dict[str, Any]],
    aggregation: dict[str, Any],
    extra: dict[str, Any] | None = None,
) -> Path:
    try:
        figure_name = str(output.relative_to(REPOSITORY_ROOT))
    except ValueError:
        figure_name = str(output)
    payload = {
        "schema_version": 1,
        "figure": figure_name,
        "git_revision": git_revision(),
        "conditions": [condition.provenance() for condition in conditions],
        "result_filters": result_filters,
        "aggregation": aggregation,
        "extra": extra or {},
    }
    sidecar = output.with_suffix(output.suffix + ".json")
    sidecar.parent.mkdir(parents=True, exist_ok=True)
    sidecar.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    return sidecar
