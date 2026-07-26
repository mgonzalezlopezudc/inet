#!/usr/bin/env python3
"""Internal adapter for scenario-scoped AX scalar summarization."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analysis-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--group", required=True)
    parser.add_argument("--session-id", required=True)
    args = parser.parse_args()

    document = json.loads(args.manifest.read_text(encoding="utf-8"))
    if set(document.get("groups", {})) != {args.group}:
        parser.error(
            "--manifest must contain exactly the selected scalar/vector group"
        )

    sys.path.insert(0, str(args.analysis_dir))
    import summarize_results

    summarize_results.DEFAULT_MANIFEST = args.manifest
    sys.argv = [
        "summarize_results.py",
        "--session-id",
        args.session_id,
    ]
    summarize_results.main()


if __name__ == "__main__":
    main()
