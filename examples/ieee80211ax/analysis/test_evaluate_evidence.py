import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch

import pandas as pd

from evaluate_evidence import (
    MissingDiagnosticTelemetryError,
    _trigger_decision_rows,
    build_ledger,
    decode_he_trigger_ru,
    evaluate_matched_delivery,
    evaluate_mimo_triplets,
    evaluate_ul_trigger_allocation_join,
)


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


class UlTriggerAllocationEvidenceTest(unittest.TestCase):
    @staticmethod
    def model_user(**overrides):
        return {
            "simulation_time": "0.5",
            "trigger_id": 17,
            "trigger_type": 0,
            "user_ordinal": 0,
            "association_id": 3,
            "backlog_bytes": 1500,
            "reported_bytes": 1000,
            "planned_bytes": 800,
            "tid": 0,
            "access_category": 1,
            "selected": 1,
            "ru_index": 1,
            "ru_tone_size": 106,
            "ru_tone_offset": 0,
            **overrides,
        }

    @staticmethod
    def packet_trigger(**overrides):
        return {
            "simulation_time": "0.500000000",
            "frame_number": 42,
            "trigger_type": 0,
            "ul_bandwidth": 0,
            "field_cardinality_consistent": True,
            "users": [{
                "ordinal": 0,
                "association_id": 3,
                "ru_allocation_region": 0,
                "ru_allocation": 53,
                "ul_mcs": 0,
                "ul_target_rssi": 35,
            }],
            **overrides,
        }

    def test_decodes_trigger_ru_allocation_to_inet_geometry(self):
        self.assertEqual(decode_he_trigger_ru(0, 53), (106, 0))
        self.assertEqual(decode_he_trigger_ru(0, 54), (106, 136))
        self.assertEqual(decode_he_trigger_ru(3, 67, 1), (996, 996))

    def test_ordered_causal_join_preserves_scheduler_inputs(self):
        result = evaluate_ul_trigger_allocation_join(
            {"BacklogBased": [self.model_user()]},
            {"BacklogBased": [self.packet_trigger(
                simulation_time="0.500036"
            )]},
        )
        self.assertEqual(result.status, "PASS")
        self.assertEqual(result.observations[0]["reported_bytes"], 1000)
        self.assertEqual(result.observations[0]["planned_bytes"], 800)
        self.assertEqual(
            result.observations[0]["commit_to_capture_delay_us"], 36.0
        )
        self.assertTrue(result.observations[0]["matched"])

    def test_noncausal_or_excessively_delayed_join_is_inconclusive(self):
        for packet_time in ("0.499999", "0.501001"):
            with self.subTest(packet_time=packet_time):
                result = evaluate_ul_trigger_allocation_join(
                    {"BacklogBased": [self.model_user()]},
                    {"BacklogBased": [self.packet_trigger(
                        simulation_time=packet_time
                    )]},
                )
                self.assertEqual(result.status, "INCONCLUSIVE")

    def test_mismatch_fails_and_ambiguity_is_inconclusive(self):
        mismatch = evaluate_ul_trigger_allocation_join(
            {"BacklogBased": [self.model_user()]},
            {"BacklogBased": [self.packet_trigger(
                users=[{
                    "ordinal": 0,
                    "association_id": 2,
                    "ru_allocation_region": 0,
                    "ru_allocation": 53,
                    "ul_mcs": 0,
                    "ul_target_rssi": 35,
                }]
            )]},
        )
        self.assertEqual(mismatch.status, "FAIL")
        ambiguity = evaluate_ul_trigger_allocation_join(
            {"BacklogBased": [self.model_user()]},
            {"BacklogBased": [
                self.packet_trigger(frame_number=42),
                self.packet_trigger(frame_number=43),
            ]},
        )
        self.assertEqual(ambiguity.status, "INCONCLUSIVE")


class EvidenceLedgerProvenanceTest(unittest.TestCase):
    def test_empty_diagnostic_vector_reports_rerun_guidance(self):
        condition = SimpleNamespace(
            config="BacklogBased",
            _read=Mock(return_value=object()),
        )
        evaluation = {
            "diagnostic_run": 0,
            "module": "**.ulCoordinator",
            "vectors": {"trigger_id": "triggerId:vector"},
        }
        with patch(
            "evaluate_evidence.scave_results.get_vectors",
            return_value=pd.DataFrame(),
        ):
            with self.assertRaises(MissingDiagnosticTelemetryError) as raised:
                _trigger_decision_rows(condition, evaluation)
        message = str(raised.exception)
        self.assertIn("BacklogBased/triggerId:vector", message)
        self.assertIn("diagnostic run 0", message)
        self.assertIn("--exhaustive-vectors", message)

    def test_missing_diagnostic_telemetry_maps_contract_to_not_run(self):
        manifest = {
            "groups": {
                "diagnostic": {"measurement": {"start": 0, "end": 1}},
            },
            "evidence_contracts": {
                "diagnostic": [{
                    "id": "diagnostic-check",
                    "kind": "model",
                    "requirement": "diagnostic telemetry",
                    "results": ["triggerId"],
                    "evaluation": {"handler": "ul_trigger_allocation_join"},
                }],
            },
        }
        condition = SimpleNamespace(
            measurement=SimpleNamespace(start=0, end=1),
            provenance=Mock(return_value={"config": "BacklogBased"}),
        )
        error = MissingDiagnosticTelemetryError(
            "BacklogBased", "triggerId:vector", 0
        )
        with (
            patch("evaluate_evidence.resolve_session_id", return_value="session"),
            patch("evaluate_evidence.conditions_for_group", return_value=[condition]),
            patch("evaluate_evidence.evaluate_contract", side_effect=error),
            patch("evaluate_evidence.git_revision", return_value="revision"),
        ):
            ledger = build_ledger(
                manifest, "session", ["diagnostic"], Path("/tmp/manifest.json")
            )
        check = ledger["groups"]["diagnostic"]["checks"][0]
        self.assertEqual(check["status"], "NOT RUN")
        self.assertEqual(ledger["groups"]["diagnostic"]["status"], "NOT RUN")
        self.assertEqual(ledger["status_summary"]["NOT RUN"], 1)
        self.assertIn("--exhaustive-vectors", check["reason"])

    def test_ledger_records_the_loaded_manifest_path(self):
        manifest = {
            "groups": {"diagnostic": {"measurement": {"start": 0, "end": 1}}},
            "evidence_contracts": {
                "diagnostic": [{
                    "id": "not-run",
                    "kind": "metric",
                    "requirement": "diagnostic",
                    "results": ["missing"],
                    "evaluation": {"handler": "unimplemented"},
                }]
            },
        }
        path = Path("/tmp/session-filtered-experiments.json")
        with patch(
            "evaluate_evidence.resolve_session_id",
            side_effect=FileNotFoundError("not retained"),
        ):
            ledger = build_ledger(
                manifest, "20260726T120000Z", ["diagnostic"], path
            )
        self.assertEqual(ledger["manifest"], str(path))


if __name__ == "__main__":
    unittest.main()
