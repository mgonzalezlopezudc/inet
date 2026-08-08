"""Declarative suite loading for generation-neutral analysis entry points."""

import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Suite:
    schema_version: int
    suite: str
    descriptor_path: Path
    example_root: Path
    scenarios: dict
    capture: dict
    generated_marker: str
    scalar_vector_manifest: Path | None = None


def scenario_configuration_ini(
    example_root: str | Path,
    scenario: dict,
    config: str,
) -> Path:
    """Return the declared INI file for one scenario configuration."""
    if config not in scenario.get("configurations", ()):
        raise ValueError(f"Unknown scenario configuration {config!r}")
    relative_ini = scenario.get("configuration_inis", {}).get(
        config,
        scenario["ini"],
    )
    return Path(example_root) / relative_ini


def load_suite(path: str | Path, repository_root: str | Path) -> Suite:
    repository_root = Path(repository_root).resolve()
    path = Path(path)
    if not path.is_absolute():
        path = repository_root / path
    path = path.resolve()
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
        scenario_parent = Path(scenario["ini"]).parent
        split_configs = sorted(
            config
            for config, ini in configuration_inis.items()
            if Path(ini).parent != scenario_parent
        )
        if split_configs:
            raise ValueError(
                f"{path}: scenario {scenario_name!r} maps configurations "
                f"outside one simulation example: {split_configs}"
            )
        scalar_vector_manifest = scenario.get("scalar_vector_manifest")
        if scalar_vector_manifest is not None and not isinstance(
            scalar_vector_manifest, str
        ):
            raise ValueError(
                f"{path}: scenario {scenario_name!r} scalar_vector_manifest "
                "must be a string"
            )
    capture = document["capture"]
    if not isinstance(capture, dict):
        raise ValueError(f"{path}: capture must be an object")
    interface_patterns = capture.get("interface_patterns")
    if not isinstance(interface_patterns, list) or not interface_patterns:
        raise ValueError(f"{path}: capture.interface_patterns must be non-empty")
    scope = capture.get("scope")
    if scope is not None and scope not in ("ap", "sta", "stas", "both"):
        raise ValueError(f"{path}: capture.scope must be 'ap', 'sta', or 'both'")
    root = repository_root / document["example_root"]
    scalar_vector_manifest = document.get("scalar_vector_manifest")
    if scalar_vector_manifest is not None:
        if not isinstance(scalar_vector_manifest, str):
            raise ValueError(f"{path}: scalar_vector_manifest must be a string")
        scalar_vector_manifest = (
            repository_root / scalar_vector_manifest
        )
    return Suite(
        schema_version=document["schema_version"],
        suite=document["suite"],
        descriptor_path=path,
        example_root=root,
        scenarios=scenarios,
        capture=document["capture"],
        generated_marker=document["generated_marker"],
        scalar_vector_manifest=scalar_vector_manifest,
    )
