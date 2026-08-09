#!/usr/bin/env python3
"""Strict result loading and aggregation for the IEEE 802.11ax analyses."""

from __future__ import annotations

import hashlib
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

import numpy as np
import pandas as pd
from omnetpp.scave import results
from scipy.stats import t


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
ANALYSIS_DIR = Path(__file__).resolve().parent
GENERAL_ANALYSIS_ROOT = ANALYSIS_DIR.parents[1] / "ieee80211" / "analysis"
if str(GENERAL_ANALYSIS_ROOT) not in sys.path:
    sys.path.insert(0, str(GENERAL_ANALYSIS_ROOT))

from inet_wifi_analysis import (
    result_configuration_directory,
    result_root,
    result_session_directory as shared_result_session_directory,
)

DEFAULT_MANIFEST = ANALYSIS_DIR / "experiments.json"
FIGURE_FILENAMES = {
    "fragmentation": "fragmentation-and-ack-overhead.png",
    "uora": "uora-dashboard.png",
    "twt": "twt-state-and-energy.png",
    "rate": "rate-adaptation-timeline.png",
    "er": "he-er-su-boundary.png",
    "puncturing": "puncturing-frequency-allocation.png",
    "mimo": "mu-mimo-spatial-stream-matrix.png",
    "bss": "bss-coloring-comparison.png",
    "width": "channel-width-dashboard.png",
    "dl_sched": "dl-scheduler-dashboard.png",
    "dl_asym": "dl-asymmetric-scheduler-dashboard.png",
    "dl_bar": "dl-bar-acknowledgment-dashboard.png",
    "bsr": "bsr-reported-vs-scheduled.png",
    "multi_tid": "multi-tid-delivery.png",
    "operating_mode": "operating-mode-delivery.png",
    "frequency_selective": "frequency-selective-delivery.png",
    "ndp_feedback": "ndp-feedback-delivery.png",
    "dense_iot": "dense-iot-delivery.png",
    "eht_features": "eht-features-delivery.png",
    "bcc_ldpc": "bcc-ldpc-delivery.png",
    "ul_mu_mimo": "ul-mu-mimo-delivery.png",
    "ul_ofdma": "ul-ofdma-delivery-delay.png",
    "dl_ul_ofdma": "dl-ul-ofdma-delivery-delay.png",
    "frame_aggregation": "frame-aggregation-delivery-delay.png",
    "block_ack": "block-ack-delivery-delay.png",
    "channel_widths": "channel-widths-delivery-delay.png",
    "guard_interval": "guard-interval-delivery-delay.png",
    "mimo_spatial_streams": "mimo-spatial-streams-delivery-delay.png",
    "preamble_modes": "preamble-modes-delivery-delay.png",
    "rate_adaptation": "rate-adaptation-delivery-delay.png",
    "wide_channels": "wide-channels-delivery-delay.png",
    "vht_mcs_256qam": "vht-mcs-256qam-delivery-delay.png",
    "extended_ampdu": "extended-ampdu-delivery-delay.png",
    "spatial_streams_8x8": "spatial-streams-8x8-delivery-delay.png",
    "vht_rate_adaptation": "vht-rate-adaptation-delivery-delay.png",
    "short_gi_vht": "short-gi-vht-delivery-delay.png",
    "dl_mu_mimo_baseline": "dl-mu-mimo-baseline-delivery-delay.png",
}
SESSION_ID_PATTERN = re.compile(r"^\d{8}T\d{6}Z$")
EVIDENCE_HANDLERS = {
    "unimplemented",
    "ampdu_policy_bounds",
    "mimo_disjoint_streams",
    "matched_delivery_ratio",
    "ul_trigger_allocation_join",
    "bsr_decision_join",
    "bss_spatial_reuse_join",
}

QUERY_OPTIONS = {
    "include_attrs": True,
    "include_runattrs": True,
    "include_itervars": True,
    "include_config_entries": True,
}


def load_manifest(path: Path = DEFAULT_MANIFEST) -> dict[str, Any]:
    with path.open(encoding="utf-8") as stream:
        manifest = json.load(stream)
    validate_result_layout(manifest)
    validate_evidence_contracts(manifest)
    return manifest


def validate_result_layout(manifest: dict[str, Any]) -> None:
    """Require every group to use its simulation example's canonical results root."""
    for group_name, group in manifest.get("groups", {}).items():
        if "result_dir" in group:
            raise RuntimeError(
                f"{group_name}: result_dir is obsolete; results are derived from ini"
            )
        if "ini" not in group:
            raise RuntimeError(f"{group_name}: missing ini")
        recording = group.get("recording")
        if recording is not None:
            if not isinstance(recording, dict):
                raise RuntimeError(
                    f"{group_name}: recording profile must be an object"
                )
            if group_name != "ul_ofdma":
                raise RuntimeError(
                    f"{group_name}: diagnostic recording profile is only "
                    f"defined for ul_ofdma"
                )
            if recording.get("profile") != (
                "performance-with-run0-diagnostic"
            ):
                raise RuntimeError(
                    f"{group_name}: unknown recording profile "
                    f"{recording.get('profile')!r}"
                )
            diagnostic_run = recording.get("diagnostic_run")
            repetitions = int(group["expected_repetitions"])
            if (
                not isinstance(diagnostic_run, int)
                or diagnostic_run < 0
                or diagnostic_run >= repetitions
            ):
                raise RuntimeError(
                    f"{group_name}: diagnostic_run must be in "
                    f"[0, {repetitions})"
                )
        roots = {result_root(REPOSITORY_ROOT, group["ini"])}
        for entry in group.get("conditions", []):
            if "result_dir" in entry:
                raise RuntimeError(
                    f"{group_name}/{entry.get('config')}: result_dir is obsolete"
                )
            roots.add(result_root(
                REPOSITORY_ROOT,
                entry.get("ini", group["ini"]),
            ))
        if len(roots) != 1:
            raise RuntimeError(
                f"{group_name}: all condition INI files must belong to one "
                "simulation example results directory"
            )


def validate_evidence_contracts(manifest: dict[str, Any]) -> None:
    groups = set(manifest.get("groups", {}))
    contracts = manifest.get("evidence_contracts")
    if not isinstance(contracts, dict) or set(contracts) != groups:
        raise RuntimeError("Evidence contracts must exist for every analysis group and no others")
    identifiers: set[str] = set()
    for group_name, requirements in contracts.items():
        if not isinstance(requirements, list) or not requirements:
            raise RuntimeError(f"{group_name}: evidence contract must contain requirements")
        condition_configs = {
            condition.get("config")
            for condition in manifest["groups"][group_name].get("conditions", [])
        }

        def require_conditions(identifier: str, configs: list[str]) -> None:
            missing = sorted(set(configs) - condition_configs)
            if missing:
                raise RuntimeError(
                    f"{identifier}: evidence-contract configuration(s) are not "
                    f"conditions in {group_name}: {missing}"
                )

        for requirement in requirements:
            identifier = requirement.get("id")
            if not isinstance(identifier, str) or not identifier.strip():
                raise RuntimeError(f"{group_name}: missing evidence-contract id")
            if identifier in identifiers:
                raise RuntimeError(f"Duplicate evidence-contract id {identifier!r}")
            identifiers.add(identifier)
            if requirement.get("kind") not in {"normative", "model", "metric"}:
                raise RuntimeError(f"{group_name}: invalid evidence-contract kind")
            if not requirement.get("requirement"):
                raise RuntimeError(f"{group_name}: missing evidence-contract requirement")
            if not isinstance(requirement.get("results"), list) or not requirement["results"]:
                raise RuntimeError(f"{group_name}: evidence-contract result list is empty")
            evaluation = requirement.get("evaluation")
            if not isinstance(evaluation, dict):
                raise RuntimeError(f"{identifier}: missing evaluation object")
            handler = evaluation.get("handler")
            if handler not in EVIDENCE_HANDLERS:
                raise RuntimeError(f"{identifier}: unknown evidence handler {handler!r}")
            if handler == "unimplemented":
                reason = evaluation.get("reason")
                if not isinstance(reason, str) or not reason.strip():
                    raise RuntimeError(f"{identifier}: unimplemented handler needs a reason")
            elif handler == "ampdu_policy_bounds":
                required = {
                    "baseline_config", "treatment_config", "module", "result",
                    "count_result",
                    "minimum_aggregate_bytes", "baseline_max_bytes",
                    "treatment_max_bytes", "minimum_subframes",
                }
                missing = required - evaluation.keys()
                if missing:
                    raise RuntimeError(f"{identifier}: missing parameters {sorted(missing)}")
                require_conditions(
                    identifier,
                    [evaluation["baseline_config"], evaluation["treatment_config"]],
                )
                for key in (
                    "minimum_aggregate_bytes", "baseline_max_bytes",
                    "treatment_max_bytes", "minimum_subframes",
                ):
                    value = evaluation[key]
                    if not isinstance(value, int) or value <= 0:
                        raise RuntimeError(f"{identifier}: {key} must be a positive integer")
            elif handler == "mimo_disjoint_streams":
                required = {"config", "module", "station_id", "stream_count", "stream_start"}
                missing = required - evaluation.keys()
                if missing:
                    raise RuntimeError(f"{identifier}: missing parameters {sorted(missing)}")
                require_conditions(identifier, [evaluation["config"]])
            elif handler == "matched_delivery_ratio":
                required = {"baseline_config", "treatment_config", "result", "minimum_ratio"}
                missing = required - evaluation.keys()
                if missing:
                    raise RuntimeError(f"{identifier}: missing parameters {sorted(missing)}")
                ratio = evaluation["minimum_ratio"]
                if not isinstance(ratio, (int, float)) or not math.isfinite(ratio) or ratio <= 0:
                    raise RuntimeError(f"{identifier}: minimum_ratio must be finite and positive")
                require_conditions(
                    identifier,
                    [evaluation["baseline_config"], evaluation["treatment_config"]],
                )
            elif handler == "ul_trigger_allocation_join":
                required = {
                    "configs", "diagnostic_run", "module",
                    "packet_metrics", "vectors",
                }
                missing = required - evaluation.keys()
                if missing:
                    raise RuntimeError(
                        f"{identifier}: missing parameters {sorted(missing)}"
                    )
                if not isinstance(evaluation["configs"], list) or not evaluation["configs"]:
                    raise RuntimeError(f"{identifier}: configs must be a nonempty list")
                require_conditions(identifier, evaluation["configs"])
                required_vectors = {
                    "trigger_id", "trigger_type", "user_ordinal",
                    "association_id", "backlog_bytes", "reported_bytes",
                    "planned_bytes", "tid", "access_category", "selected",
                    "ru_index", "ru_tone_size", "ru_tone_offset",
                }
                if set(evaluation["vectors"]) != required_vectors:
                    raise RuntimeError(
                        f"{identifier}: vectors must define "
                        f"{sorted(required_vectors)}"
                    )
                require_conditions(identifier, evaluation["configs"])
            elif handler == "bsr_decision_join":
                required = {"configs", "module", "vectors"}
                missing = required - evaluation.keys()
                if missing:
                    raise RuntimeError(
                        f"{identifier}: missing parameters {sorted(missing)}"
                    )
                required_vectors = {
                    "trigger_id", "trigger_type", "user_ordinal",
                    "association_id", "reported_bytes", "planned_bytes",
                }
                if set(evaluation["vectors"]) != required_vectors:
                    raise RuntimeError(
                        f"{identifier}: vectors must define "
                        f"{sorted(required_vectors)}"
                    )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def atomic_write_text(path: Path, content: str) -> None:
    """Replace a text artifact only after its complete content is durable."""
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        dir=path.parent, prefix=f".{path.name}.", suffix=".tmp", text=True
    )
    temporary_path = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
    except Exception:
        temporary_path.unlink(missing_ok=True)
        raise


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
        session_id: str,
    ) -> "Condition":
        root = REPOSITORY_ROOT
        window_data = entry.get("measurement", group["measurement"])
        ini = root / entry.get("ini", group["ini"])
        return cls(
            group=group_name,
            label=entry["label"],
            config=entry["config"],
            ini=ini,
            result_dir=result_configuration_directory(
                root,
                ini,
                session_id,
                entry["config"],
            ),
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
            rf"^{re.escape(self.config)}(?:-[^#]+)?-#"
            rf"(?P<run>\d+)\.(?P<extension>sca|vec)$"
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
        allow_missing_runs: bool = False,
    ) -> pd.DataFrame:
        expression = f'module =~ "{module}" AND name =~ "{name}"'
        frame = results.get_vectors(
            self._read(expression),
            omit_empty_vectors=True,
            **QUERY_OPTIONS,
        )
        self._validate_common(frame, name, required, allow_missing_runs)
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
        self, frame: pd.DataFrame, name: str, required: bool,
        allow_missing_runs: bool = False,
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
        if (not allow_missing_runs and run_numbers != expected) or (
            allow_missing_runs and not run_numbers.issubset(expected)
        ):
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


def _session_is_complete(
    root: Path,
    group: dict[str, Any],
    session_id: str,
) -> bool:
    for entry in group["conditions"]:
        result_dir = result_configuration_directory(
            root,
            entry.get("ini", group["ini"]),
            session_id,
            entry["config"],
        )
        repetitions = int(entry.get(
            "expected_repetitions", group["expected_repetitions"]
        ))
        for run in range(repetitions):
            for extension in ("sca", "vec"):
                matches = list(
                    result_dir.glob(
                        f"{entry['config']}*-#{run}.{extension}"
                    )
                )
                if len(matches) != 1:
                    return False
    return True


def resolve_session_id(
    group: dict[str, Any],
    requested_session_id: str | None = None,
    *,
    root: Path = REPOSITORY_ROOT,
) -> str:
    if requested_session_id is not None:
        if not SESSION_ID_PATTERN.fullmatch(requested_session_id):
            raise ValueError(
                f"Invalid session ID {requested_session_id!r}; "
                "expected YYYYMMDDTHHMMSSZ"
            )
        candidates = [requested_session_id]
    else:
        result_roots = {
            result_root(root, entry.get("ini", group["ini"]))
            for entry in group["conditions"]
        }
        candidate_sets = []
        for candidate_root in result_roots:
            candidate_sets.append({
                path.name
                for path in candidate_root.iterdir()
                if path.is_dir() and SESSION_ID_PATTERN.fullmatch(path.name)
            } if candidate_root.is_dir() else set())
        candidates = sorted(
            set.intersection(*candidate_sets) if candidate_sets else set(),
            reverse=True,
        )

    for candidate in candidates:
        if _session_is_complete(root, group, candidate):
            return candidate
    if requested_session_id is not None:
        raise FileNotFoundError(
            f"Result session {requested_session_id} is missing or incomplete"
        )
    raise FileNotFoundError(
        "No complete timestamped result session found; "
        "expected results/YYYYMMDDTHHMMSSZ/<configuration>"
    )


def resolve_manifest_sessions(
    manifest: dict[str, Any],
    requested_session_id: str | None = None,
    *,
    root: Path = REPOSITORY_ROOT,
) -> tuple[dict[str, str], dict[str, str]]:
    """Resolve each group's session without silently mixing partial sessions."""
    selected: dict[str, str] = {}
    unavailable: dict[str, str] = {}
    for group_name, group in manifest["groups"].items():
        try:
            selected[group_name] = resolve_session_id(
                group, requested_session_id, root=root
            )
        except (FileNotFoundError, ValueError) as error:
            unavailable[group_name] = str(error)
    if requested_session_id is not None and unavailable:
        details = "; ".join(
            f"{group}: {reason}" for group, reason in sorted(unavailable.items())
        )
        raise FileNotFoundError(
            f"Requested session {requested_session_id} is not complete for every "
            f"analysis group: {details}"
        )
    return selected, unavailable


def result_session_directory(
    group: dict[str, Any],
    session_id: str,
    *,
    root: Path = REPOSITORY_ROOT,
) -> Path:
    directories = {
        shared_result_session_directory(
            root,
            entry.get("ini", group["ini"]),
            session_id,
        )
        for entry in group["conditions"]
    }
    if len(directories) != 1:
        raise RuntimeError(
            "An analysis group must use one shared result session directory"
        )
    return directories.pop()


def conditions_for_group(
    manifest: dict[str, Any],
    group_name: str,
    session_id: str | None = None,
) -> list[Condition]:
    try:
        group = manifest["groups"][group_name]
    except KeyError as error:
        raise KeyError(f"Unknown analysis group {group_name!r}") from error
    selected_session_id = resolve_session_id(group, session_id)
    print(f"Using result session {selected_session_id} for {group_name}")
    return [
        Condition.from_manifest(group_name, group, entry, selected_session_id)
        for entry in group["conditions"]
    ]


def crop_vector(
    times: Iterable[float], values: Iterable[float], window: MeasurementWindow
) -> tuple[np.ndarray, np.ndarray]:
    times_array = np.asarray(times, dtype=float)
    values_array = np.asarray(values, dtype=float)
    if len(times_array) != len(values_array):
        raise ValueError("Vector timestamps and values are unaligned")
    selected = (times_array >= window.start) & (times_array < window.end)
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
    sink_pattern = condition.condition_metadata.get(
        "sink_module_regex", r"\.(?:host|sta\d*|sta)\[\d+\]\.app\["
    )
    frame = frame[frame.module.str.contains(sink_pattern, regex=True)]
    if frame.empty:
        raise RuntimeError(
            f"{condition.config}: no delay sinks matched {sink_pattern!r}"
        )
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
    condition_summaries = []
    for condition in conditions:
        item = condition.provenance()
        item.pop("result_files", None)
        condition_summaries.append(item)
    payload = {
        "schema_version": 1,
        "git_revision": git_revision(),
        "conditions": condition_summaries,
        "result_filters": result_filters,
        "aggregation": aggregation,
        "extra": extra or {},
    }
    sidecar = output.with_suffix(output.suffix + ".json")
    sidecar.parent.mkdir(parents=True, exist_ok=True)
    sidecar.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    return sidecar
