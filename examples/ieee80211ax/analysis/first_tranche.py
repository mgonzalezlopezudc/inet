#!/usr/bin/env python3
"""Generate rigorously selected IEEE 802.11ax analysis figures."""

from __future__ import annotations

import argparse
import tempfile
from pathlib import Path

from analysis_core import DEFAULT_MANIFEST, REPOSITORY_ROOT, conditions_for_group, load_manifest, sha256
from analysis_plots import PLOTS


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("group", choices=sorted(PLOTS) + ["all"])
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--check", action="store_true", help="render to a temporary directory and compare checked-in PNGs")
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    groups = sorted(PLOTS) if args.group == "all" else [args.group]
    for group_name in groups:
        group = manifest["groups"][group_name]
        checked_output = REPOSITORY_ROOT / group["output"]
        if args.check:
            with tempfile.TemporaryDirectory(prefix="inet-80211ax-analysis-") as directory:
                output = Path(directory) / checked_output.name
                PLOTS[group_name](conditions_for_group(manifest, group_name), output)
                if not checked_output.exists() or sha256(output) != sha256(checked_output):
                    raise RuntimeError(f"stale checked-in figure: {checked_output}")
                print(f"VERIFIED {checked_output}")
        else:
            PLOTS[group_name](conditions_for_group(manifest, group_name), checked_output)


if __name__ == "__main__":
    main()
