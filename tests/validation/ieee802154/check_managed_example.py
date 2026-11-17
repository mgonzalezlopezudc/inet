#!/usr/bin/env python3
"""Check the two ManagedPan runs exported with opp_scavetool -F CSV-R."""

import argparse
import csv
import json
from pathlib import Path


def check(path):
    runs = {}
    with path.open(newline="") as source:
        for row in csv.DictReader(source):
            run = runs.setdefault(row["run"], {"attributes": {}, "scalars": {}})
            if row["type"] == "runattr":
                run["attributes"][row["attrname"]] = row["attrvalue"]
            elif row["type"] == "scalar":
                key = (row["module"], row["name"])
                if key in run["scalars"]:
                    raise ValueError(f"Duplicate scalar {row['run']}: {key}")
                run["scalars"][key] = float(row["value"])
    expected = {
        (f"ManagedPan.{node}.app", metric): value
        for node, values in {
            "coordinator": {"completedCycles": 0, "receivedMsdus": 2, "confirmedMsdus": 2},
            "device": {"completedCycles": 2, "receivedMsdus": 2, "confirmedMsdus": 2},
        }.items()
        for metric, value in values.items()
    }
    if len(runs) != 2:
        raise ValueError(f"Expected exactly two typed/raw runs, found {len(runs)}")
    variants = set()
    for name, run in runs.items():
        variant = run["attributes"].get("iterationvars")
        if variant not in {"$raw=false", "$raw=true"} or variant in variants:
            raise ValueError(f"Missing or duplicate raw-byte variant in {name}: {variant}")
        variants.add(variant)
        if run["scalars"] != expected:
            raise ValueError(f"Unexpected managed exchange scalars in {name}: {run['scalars']}")
    return {"result": "PASS", "runs": len(runs), "scalars": len(expected) * len(runs)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    args = parser.parse_args()
    print(json.dumps(check(args.csv), sort_keys=True))
