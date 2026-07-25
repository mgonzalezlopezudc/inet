#!/usr/bin/env python3

import math
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))

from analysis_core import (
    MeasurementWindow,
    atomic_write_text,
    crop_vector,
    jain,
    resolve_session_id,
    resolve_manifest_sessions,
    summarize_ci95,
    time_weighted_integral,
    validate_disjoint_streams,
    validate_evidence_contracts,
    validate_unpunctured_ru,
)
from run_campaign import (
    available_cpu_count,
    collect_jobs,
    positive_int,
    run_jobs,
    session_id,
)


class AnalysisCoreTest(unittest.TestCase):
    def test_window_and_crop_are_explicit(self):
        window = MeasurementWindow(1, 3)
        times, values = crop_vector([0, 1, 2, 3, 4], [10, 11, 12, 13, 14], window)
        self.assertEqual(times.tolist(), [1, 2, 3])
        self.assertEqual(values.tolist(), [11, 12, 13])

    def test_piecewise_constant_energy_includes_window_edges(self):
        self.assertAlmostEqual(
            time_weighted_integral([0, 1, 2.5], [2, 4, 8], MeasurementWindow(0.5, 3)),
            11,
        )

    def test_statistics_use_independent_run_values(self):
        result = summarize_ci95([1, 2, 3, 4, 5])
        self.assertEqual(result["count"], 5)
        self.assertAlmostEqual(result["mean"], 3)
        self.assertGreater(result["ci95"], 0)
        self.assertAlmostEqual(jain([1, 1, 1]), 1)
        self.assertTrue(math.isnan(jain([0, 0])))

    def test_stream_overlap_is_rejected(self):
        validate_disjoint_streams([1, 2], [0, 2], [2, 2])
        with self.assertRaises(RuntimeError):
            validate_disjoint_streams([1, 2], [0, 1], [2, 2])

    def test_ru_overlap_is_rejected(self):
        validate_unpunctured_ru(0, 100, [(100, 200)])
        with self.assertRaises(RuntimeError):
            validate_unpunctured_ru(50, 100, [(100, 200)])

    def test_every_group_requires_an_evidence_contract(self):
        manifest = {
            "groups": {"sample": {}},
            "evidence_contracts": {
                "sample": [{
                    "id": "sample-check",
                    "kind": "normative",
                    "requirement": "observable invariant",
                    "results": ["signal"],
                    "evaluation": {
                        "handler": "unimplemented",
                        "reason": "The decisive signal is not recorded.",
                    },
                }]
            },
        }
        validate_evidence_contracts(manifest)
        duplicate = {
            "groups": {"first": {}, "second": {}},
            "evidence_contracts": {
                "first": manifest["evidence_contracts"]["sample"],
                "second": manifest["evidence_contracts"]["sample"],
            },
        }
        with self.assertRaisesRegex(RuntimeError, "Duplicate"):
            validate_evidence_contracts(duplicate)
        manifest["evidence_contracts"] = {}
        with self.assertRaises(RuntimeError):
            validate_evidence_contracts(manifest)


class CampaignRunnerTest(unittest.TestCase):
    SESSION_ID = "20260725T120000Z"
    MANIFEST = {
        "groups": {
            "sample": {
                "ini": "sample/omnetpp.ini",
                "result_dir": "sample/results",
                "expected_repetitions": 2,
                "conditions": [
                    {"config": "First"},
                    {"config": "Second", "result_dir": "other/results"},
                ],
            }
        }
    }

    def test_campaign_expands_configurations_and_repetitions_into_jobs(self):
        jobs = collect_jobs(
            self.MANIFEST, "sample", campaign_session_id=self.SESSION_ID
        )
        self.assertEqual(
            [(job.config, job.run) for job in jobs],
            [("First", 0), ("First", 1), ("Second", 0), ("Second", 1)],
        )
        self.assertIn("--seed-set=1", jobs[1].command)
        self.assertIn("--repeat=2", jobs[1].command)
        result_argument = next(
            value for value in jobs[2].command
            if value.startswith("--result-dir=")
        )
        self.assertIn(
            f"other/results/scalar-vector/{self.SESSION_ID}/Second",
            result_argument,
        )

    def test_campaign_filters_configs_and_overrides_repetitions(self):
        jobs = collect_jobs(
            self.MANIFEST, "sample", 3, {"Second"}, self.SESSION_ID
        )
        self.assertEqual([(job.config, job.run) for job in jobs], [("Second", 0), ("Second", 1), ("Second", 2)])

    def test_parallel_limit_is_bounded_by_job_count(self):
        jobs = collect_jobs(
            self.MANIFEST, "sample", campaign_session_id=self.SESSION_ID
        )[:2]
        with patch("run_campaign.ThreadPoolExecutor") as executor:
            executor.return_value.__enter__.return_value.submit.side_effect = RuntimeError("stop after pool creation")
            with self.assertRaisesRegex(RuntimeError, "stop after pool creation"):
                run_jobs(jobs, 99)
        executor.assert_called_once_with(max_workers=2, thread_name_prefix="simulation")

    def test_positive_limits_and_cpu_default(self):
        self.assertEqual(positive_int("3"), 3)
        with self.assertRaises(Exception):
            positive_int("0")
        self.assertGreaterEqual(available_cpu_count(), 1)

    def test_session_id_format(self):
        self.assertEqual(session_id(self.SESSION_ID), self.SESSION_ID)
        with self.assertRaises(Exception):
            session_id("2026-07-25")

    def test_latest_complete_common_session_is_selected(self):
        group = self.MANIFEST["groups"]["sample"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for selected_session, configs in (
                ("20260724T120000Z", ("First", "Second")),
                ("20260725T120000Z", ("First",)),
            ):
                for config in configs:
                    base = "other/results" if config == "Second" else "sample/results"
                    result_dir = (
                        root
                        / base
                        / "scalar-vector"
                        / selected_session
                        / config
                    )
                    result_dir.mkdir(parents=True)
                    for run in range(2):
                        (result_dir / f"{config}-#{run}.sca").touch()
                        (result_dir / f"{config}-#{run}.vec").touch()
            self.assertEqual(
                resolve_session_id(group, root=root),
                "20260724T120000Z",
            )
            with self.assertRaises(FileNotFoundError):
                resolve_session_id(
                    group, "20260725T120000Z", root=root
                )

    def test_explicit_manifest_session_must_cover_every_group(self):
        manifest = {
            "groups": {
                "first": self.MANIFEST["groups"]["sample"],
                "second": self.MANIFEST["groups"]["sample"],
            }
        }
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(FileNotFoundError, "every analysis group"):
                resolve_manifest_sessions(
                    manifest, self.SESSION_ID, root=Path(directory)
                )

    def test_atomic_text_write_replaces_complete_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "metrics.json"
            output.write_text("stale")
            atomic_write_text(output, '{"fresh": true}\n')
            self.assertEqual(output.read_text(), '{"fresh": true}\n')
            self.assertEqual(list(output.parent.glob(".metrics.json.*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
