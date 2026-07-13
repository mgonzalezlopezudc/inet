#!/usr/bin/env python3

import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from analysis_core import (
    MeasurementWindow,
    crop_vector,
    jain,
    summarize_ci95,
    time_weighted_integral,
    validate_disjoint_streams,
    validate_unpunctured_ru,
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


if __name__ == "__main__":
    unittest.main()
