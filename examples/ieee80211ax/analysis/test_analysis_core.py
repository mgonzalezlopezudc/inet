#!/usr/bin/env python3

import math
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))

from analysis_core import (
    MeasurementWindow,
    crop_vector,
    jain,
    summarize_ci95,
    time_weighted_integral,
    validate_disjoint_streams,
    validate_evidence_contracts,
    validate_unpunctured_ru,
)
from run_campaign import available_cpu_count, collect_jobs, positive_int, run_jobs


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
                "sample": [{"kind": "normative", "requirement": "observable invariant", "results": ["signal"]}]
            },
        }
        validate_evidence_contracts(manifest)
        manifest["evidence_contracts"] = {}
        with self.assertRaises(RuntimeError):
            validate_evidence_contracts(manifest)


class CampaignRunnerTest(unittest.TestCase):
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
        jobs = collect_jobs(self.MANIFEST, "sample")
        self.assertEqual(
            [(job.config, job.run) for job in jobs],
            [("First", 0), ("First", 1), ("Second", 0), ("Second", 1)],
        )
        self.assertIn("--seed-set=1", jobs[1].command)
        self.assertIn("--repeat=2", jobs[1].command)
        self.assertIn("other/results", next(value for value in jobs[2].command if value.startswith("--result-dir=")))

    def test_campaign_filters_configs_and_overrides_repetitions(self):
        jobs = collect_jobs(self.MANIFEST, "sample", 3, {"Second"})
        self.assertEqual([(job.config, job.run) for job in jobs], [("Second", 0), ("Second", 1), ("Second", 2)])

    def test_parallel_limit_is_bounded_by_job_count(self):
        jobs = collect_jobs(self.MANIFEST, "sample")[:2]
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


if __name__ == "__main__":
    unittest.main()
