import unittest

from evaluate_evidence import evaluate_matched_delivery, evaluate_mimo_triplets


class MimoEvidenceTest(unittest.TestCase):
    def test_disjoint_multi_user_streams_pass_for_every_run(self):
        result = evaluate_mimo_triplets({
            0: [(0.5, 1, 2, 0), (0.5, 2, 1, 2)],
            1: [(0.6, 1, 1, 0), (0.6, 2, 2, 1)],
        })
        self.assertEqual(result.status, "PASS")
        self.assertEqual(result.observations[0]["multi_user_ppdu_count"], 1)

    def test_overlap_and_missing_multi_user_ppdu_fail(self):
        overlap = evaluate_mimo_triplets({
            0: [(0.5, 1, 2, 0), (0.5, 2, 2, 1)],
        })
        self.assertEqual(overlap.status, "FAIL")
        single_user = evaluate_mimo_triplets({0: [(0.5, 1, 2, 0)]})
        self.assertEqual(single_user.status, "FAIL")


class MatchedDeliveryEvidenceTest(unittest.TestCase):
    def test_matched_delivery_threshold_is_per_run(self):
        result = evaluate_matched_delivery(
            {(0, 10): 100, (1, 11): 200},
            {(0, 10): 95, (1, 11): 190},
            0.95,
        )
        self.assertEqual(result.status, "PASS")
        failed = evaluate_matched_delivery(
            {(0, 10): 100, (1, 11): 200},
            {(0, 10): 94, (1, 11): 200},
            0.95,
        )
        self.assertEqual(failed.status, "FAIL")

    def test_pair_mismatch_and_zero_baseline_are_inconclusive(self):
        mismatch = evaluate_matched_delivery({(0, 10): 100}, {(0, 11): 100}, 0.95)
        self.assertEqual(mismatch.status, "INCONCLUSIVE")
        zero = evaluate_matched_delivery({(0, 10): 0}, {(0, 10): 0}, 0.95)
        self.assertEqual(zero.status, "INCONCLUSIVE")


if __name__ == "__main__":
    unittest.main()
