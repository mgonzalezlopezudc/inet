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
    interface_patterns = document["capture"].get("interface_patterns")
    if not isinstance(interface_patterns, list) or not interface_patterns:
        raise ValueError(f"{path}: capture.interface_patterns must be non-empty")
    root = Path(repository_root) / document["example_root"]
    return Suite(
        schema_version=document["schema_version"],
        suite=document["suite"],
        example_root=root,
        scenarios=scenarios,
        capture=document["capture"],
        generated_marker=document["generated_marker"],
    )

