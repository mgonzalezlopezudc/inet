"""Declarative suite loading for generation-neutral analysis entry points."""

import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Suite:
    schema_version: int
    suite: str
    example_root: Path
    scenarios: dict
    capture: dict
    generated_marker: str
    scalar_vector_manifest: Path | None = None


def load_suite(path: str | Path, repository_root: str | Path) -> Suite:
    path = Path(path)
    document = json.loads(path.read_text(encoding="utf-8"))
    required = {
        "schema_version",
        "suite",
        "example_root",
        "scenarios",
        "capture",
        "generated_marker",
    }
    missing = sorted(required - document.keys())
    if missing:
        raise ValueError(f"{path}: missing suite fields: {', '.join(missing)}")
    if document["schema_version"] != 1:
        raise ValueError(f"{path}: unsupported schema_version")
    scenarios = document["scenarios"]
    if not isinstance(scenarios, dict) or not scenarios:
        raise ValueError(f"{path}: scenarios must be a non-empty object")
    for scenario_name, scenario in scenarios.items():
        if not isinstance(scenario, dict):
            raise ValueError(f"{path}: scenario {scenario_name!r} must be an object")
        scalar_vector_group = scenario.get("scalar_vector_group")
        if scalar_vector_group is not None and not isinstance(
            scalar_vector_group, str
        ):
            raise ValueError(
                f"{path}: scenario {scenario_name!r} scalar_vector_group "
                "must be a string"
            )
        configuration_inis = scenario.get("configuration_inis", {})
        if not isinstance(configuration_inis, dict) or any(
            not isinstance(config, str) or not isinstance(ini, str)
            for config, ini in configuration_inis.items()
        ):
            raise ValueError(
                f"{path}: scenario {scenario_name!r} configuration_inis "
                "must map configuration names to INI paths"
            )
        unknown_ini_configs = (
            set(configuration_inis) - set(scenario.get("configurations", []))
        )
        if unknown_ini_configs:
            raise ValueError(
                f"{path}: scenario {scenario_name!r} configuration_inis "
                f"contains unknown configurations: {sorted(unknown_ini_configs)}"
            )
        scalar_vector_manifest = scenario.get("scalar_vector_manifest")
        if scalar_vector_manifest is not None and not isinstance(
            scalar_vector_manifest, str
        ):
            raise ValueError(
                f"{path}: scenario {scenario_name!r} scalar_vector_manifest "
                "must be a string"
            )
    interface_patterns = document["capture"].get("interface_patterns")
    if not isinstance(interface_patterns, list) or not interface_patterns:
        raise ValueError(f"{path}: capture.interface_patterns must be non-empty")
    root = Path(repository_root) / document["example_root"]
    scalar_vector_manifest = document.get("scalar_vector_manifest")
    if scalar_vector_manifest is not None:
        if not isinstance(scalar_vector_manifest, str):
            raise ValueError(f"{path}: scalar_vector_manifest must be a string")
        scalar_vector_manifest = (
            Path(repository_root) / scalar_vector_manifest
        )
    return Suite(
        schema_version=document["schema_version"],
        suite=document["suite"],
        example_root=root,
        scenarios=scenarios,
        capture=document["capture"],
        generated_marker=document["generated_marker"],
        scalar_vector_manifest=scalar_vector_manifest,
    )
