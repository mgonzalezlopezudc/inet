#!/usr/bin/env python3
"""Diagnostic only. Calls pinned native test APIs without changing baselines."""
import argparse
import json
import re
import subprocess
import sys
import traceback
from pathlib import Path

CASES = [
    ("examples/wireless/corruptionmode", "BitCorruptionMode", "10s"),
    ("examples/wireless/corruptionmode", "ByteCorruptionMode", "10s"),
    ("examples/wireless/corruptionmode", "ChunkCorruptionMode", "10s"),
    ("examples/wireless/corruptionmode", "PacketCorruptionMode", "10s"),
    ("examples/wireless/scaling", "DimensionalRadioMedium", "0.1s"),
    ("examples/wireless/scaling", "ScalarRadioMedium", "0.1s"),
    ("showcases/tsn/combiningfeatures/gsandcs", "SAT", "0.2s"),
    ("showcases/visualizer/canvas/instrumentfigures", "General", "3s"),
    ("showcases/wireless/analogmodel", "Noise", "0.1s"),
    ("tutorials/wireless", "Wireless12", "20s"),
    ("tutorials/wireless", "Wireless13", "20s"),
    ("tutorials/wireless", "Wireless14", "20s"),
]
SAT_FOLDER = "showcases/tsn/combiningfeatures/gsandcs"


def save(path, value):
    path.write_text(json.dumps(value, indent=2, default=str) + "\n")


def leaves(result):
    if hasattr(result, "results"):
        for child in result.results:
            yield from leaves(child)
    else:
        yield result


def native_options(project, mode, folder, config):
    return dict(simulation_project=project, mode=mode, build=False, concurrent=False,
                working_directory_filter="^" + re.escape(folder) + "$",
                ini_file_filter=r"^omnetpp\.ini$", config_filter="^" + re.escape(config) + "$",
                run_number=0, full_match=True)


def run(mode, out):
    from opp_repl.simulation.workspace import load_opp_file, determine_default_simulation_project
    from opp_repl.test.fingerprint import run_fingerprint_tests
    from opp_repl.test.statistical import run_statistical_tests
    load_opp_file(".")
    project = determine_default_simulation_project(name="inet")
    out.mkdir(parents=True, exist_ok=True)
    manifest = {"mode": mode, "inet": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                "python": sys.version, "fingerprints": [], "scalars": [], "errors": []}
    for repeat in range(2):
        try:
            result = run_fingerprint_tests(**native_options(project, mode, SAT_FOLDER, "SAT"))
            entries = []
            for leaf in leaves(result):
                entries.append({"ingredients": leaf.task.ingredients,
                                "actual": str(leaf.calculated_fingerprint),
                                "stored": str(leaf.expected_fingerprint),
                                "result": leaf.result, "reason": leaf.reason,
                                "parameters": leaf.task.get_parameters_string(),
                                "sim_time_limit": leaf.task.sim_time_limit})
            if len(entries) != 3 or {x["ingredients"] for x in entries} != {"tplx", "~tNl", "~tND"}:
                raise RuntimeError("SAT fingerprint selection must contain exactly the three native ingredients")
            if any(x["actual"] == "None" or str(x["sim_time_limit"]) != "0.2s" for x in entries):
                raise RuntimeError("SAT fingerprint execution did not produce the three actual values at 0.2s")
            manifest["fingerprints"].append({"repeat": repeat, "entries": entries})
            save(out / (f"fingerprint-{repeat}.json"), result.to_dict())
        except Exception:
            manifest["errors"].append({"fingerprint_repeat": repeat, "traceback": traceback.format_exc()})
        save(out / "manifest.json", manifest)
    if mode == "release":
        for index, (folder, config, expected_limit) in enumerate(CASES):
            for repeat in range(2 if config == "SAT" else 1):
                key = f"case-{index:02d}-{repeat}"
                try:
                    result = run_statistical_tests(**native_options(project, mode, folder, config))
                    entries = list(leaves(result))
                    if len(entries) != 1:
                        raise RuntimeError(f"Expected exactly one scalar case, selected {len(entries)}")
                    leaf = entries[0]
                    limit = str(leaf.task.simulation_task.sim_time_limit)
                    if limit != expected_limit:
                        raise RuntimeError(f"Wrong simulation limit: {limit} != {expected_limit}")
                    if getattr(leaf, "current_df", None) is None or leaf.current_df.empty:
                        raise RuntimeError(f"No current scalar data: {leaf.result}: {leaf.reason}")
                    leaf.current_df.to_pickle(out / (key + ".pickle"))
                    leaf.current_df.to_csv(out / (key + ".csv"), index=False, float_format="%.17g")
                    if leaf.stored_df is not None:
                        leaf.stored_df.to_csv(out / (key + "-stored.csv"), index=False, float_format="%.17g")
                    save(out / (key + "-native.json"), result.to_dict())
                    manifest["scalars"].append({"key": key, "folder": folder, "config": config,
                                                "repeat": repeat, "sim_time_limit": limit,
                                                "rows": len(leaf.current_df), "result": leaf.result,
                                                "reason": leaf.reason})
                except Exception:
                    manifest["errors"].append({"key": key, "traceback": traceback.format_exc()})
                save(out / "manifest.json", manifest)
    if manifest["errors"]:
        print(json.dumps(manifest["errors"], indent=2))
        return 2
    return 0


def compare(mode, out):
    import pandas as pd
    from opp_repl.common.util import compare_scalar_dataframes
    comparison = {"mode": mode, "fingerprints": [], "scalars": [], "repeat_controls": [], "errors": []}
    manifests = {}
    for label in ["parent", "head"]:
        file = out / label / "manifest.json"
        if not file.exists():
            comparison["errors"].append(f"Missing {label} manifest")
        else:
            manifests[label] = json.loads(file.read_text())
            comparison["errors"].extend(manifests[label]["errors"])
    if len(manifests) != 2:
        save(out / "comparison.json", comparison)
        return 2
    for repeat in range(2):
        values = {label: {e["ingredients"]: e["actual"] for r in m["fingerprints"] if r["repeat"] == repeat for e in r["entries"]}
                  for label, m in manifests.items()}
        comparison["fingerprints"].append({"repeat": repeat, "values": values,
                                           "equal": bool(values["parent"]) and values["parent"] == values["head"]})
    for label, manifest in manifests.items():
        values = [{e["ingredients"]: e["actual"] for e in r["entries"]} for r in manifest["fingerprints"]]
        comparison["repeat_controls"].append({"label": label, "type": "SAT fingerprints", "equal": len(values) == 2 and values[0] == values[1]})

    def scalar_difference(a, b, stem):
        a_df, b_df = pd.read_pickle(a), pd.read_pickle(b)
        c = compare_scalar_dataframes(a_df, b_df, suffixes=("_parent", "_head"))
        c.different.to_csv(out / (stem + "-different.csv"), index=False, float_format="%.17g")
        c.only_parent.to_csv(out / (stem + "-only-parent.csv"), index=False, float_format="%.17g")
        c.only_head.to_csv(out / (stem + "-only-head.csv"), index=False, float_format="%.17g")
        return {"different": len(c.different), "only_parent": len(c.only_parent), "only_head": len(c.only_head),
                "equal": c.different.empty and c.only_parent.empty and c.only_head.empty}

    if mode == "release":
        for index, (folder, config, _) in enumerate(CASES):
            for repeat in range(2 if config == "SAT" else 1):
                key = f"case-{index:02d}-{repeat}"
                a, b = out / "parent" / (key + ".pickle"), out / "head" / (key + ".pickle")
                if not a.exists() or not b.exists():
                    comparison["errors"].append(f"Missing actual scalar dataframe for {key}")
                    continue
                comparison["scalars"].append({"folder": folder, "config": config, "repeat": repeat,
                                              **scalar_difference(a, b, key)})
        for label in ["parent", "head"]:
            a, b = out / label / "case-06-0.pickle", out / label / "case-06-1.pickle"
            if a.exists() and b.exists():
                comparison["repeat_controls"].append({"label": label, "type": "SAT scalars",
                                                       **scalar_difference(a, b, label + "-SAT-repeat")})
    save(out / "comparison.json", comparison)
    print(json.dumps(comparison, indent=2))
    if comparison["errors"]:
        return 2
    return 0 if all(x["equal"] for kind in ["fingerprints", "scalars", "repeat_controls"] for x in comparison[kind]) else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("operation", choices=["run", "compare"])
    parser.add_argument("--mode", choices=["debug", "release"], required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    raise SystemExit(run(args.mode, args.out) if args.operation == "run" else compare(args.mode, args.out))
