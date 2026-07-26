import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import sys

ANALYSIS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ANALYSIS_ROOT))

import wifi_analysis


AX_SUITE = ANALYSIS_ROOT / "suites" / "ax.json"
BE_SUITE = ANALYSIS_ROOT / "suites" / "be-eht-features.json"
SESSION = "20260726T120000Z"


class WifiAnalysisCliTest(unittest.TestCase):

    def test_ax_mapped_groups_are_available_to_both_pipelines(self):
        suite = wifi_analysis.load_suite(
            AX_SUITE, wifi_analysis.REPOSITORY_ROOT
        )
        self.assertEqual(
            sum(
                "scalar_vector_group" in scenario
                for scenario in suite.scenarios.values()
            ),
            11,
        )
        for scenario_name, scenario in suite.scenarios.items():
            if "scalar_vector_group" not in scenario:
                continue
            _, group, document = wifi_analysis.scalar_document(
                suite, scenario_name, scenario
            )
            self.assertTrue(
                wifi_analysis.scalar_configs(document, group).issubset(
                    scenario["configurations"]
                ),
                scenario_name,
            )

    def test_inspect_is_read_only(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            args = SimpleNamespace(
                suite=str(AX_SUITE), scenario="twt", session_id=None
            )
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(wifi_analysis.subprocess, "run") as run,
                redirect_stdout(io.StringIO()) as output,
            ):
                wifi_analysis.inspect_command(args)
            document = json.loads(output.getvalue())
            self.assertEqual(document["scalar_vector"]["group"], "twt")
            self.assertEqual(
                document["scalar_vector"]["measurement_window_s"]["notation"],
                "[start, end)",
            )
            self.assertFalse(generated.exists())
            run.assert_not_called()

    def test_inspect_existing_session_is_still_read_only(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            session_dir = generated / "sessions" / SESSION
            session_dir.mkdir(parents=True)
            (session_dir / "session.json").write_text(json.dumps({
                "suite": "ax",
                "scenario": "twt",
                "runs": 1,
                "classification": "diagnostic",
            }))
            args = SimpleNamespace(
                suite=str(AX_SUITE), scenario="twt", session_id=SESSION
            )
            before = (session_dir / "session.json").read_bytes()
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(wifi_analysis.subprocess, "run") as run,
                redirect_stdout(io.StringIO()) as output,
            ):
                wifi_analysis.inspect_command(args)
            document = json.loads(output.getvalue())
            self.assertFalse(document["session"]["publication_ready"])
            self.assertEqual(
                (session_dir / "session.json").read_bytes(), before
            )
            run.assert_not_called()

    def test_run_uses_one_session_and_writes_logical_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            args = SimpleNamespace(
                suite=str(AX_SUITE),
                scenario="twt",
                evidence="both",
                runs=5,
                config=None,
                jobs=2,
                session_id=SESSION,
            )
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(wifi_analysis, "run_command") as run,
                redirect_stdout(io.StringIO()),
            ):
                wifi_analysis.run_command_handler(args)
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(len(commands), 2)
            self.assertIn("run_campaign.py", commands[0][1])
            self.assertEqual(
                Path(commands[1][1]),
                ANALYSIS_ROOT / "analyze_pcap.py",
            )
            self.assertIn("--capture-only", commands[1])
            self.assertTrue(
                all(
                    command[command.index("--session-id") + 1] == SESSION
                    for command in commands
                )
            )
            logical = json.loads(
                (generated / "sessions" / SESSION / "session.json").read_text()
            )
            self.assertEqual(logical["classification"], "publication")
            self.assertEqual(logical["run_range"]["notation"], "[start, end)")
            self.assertEqual(
                set(logical["configurations"]),
                {"BaselineEnergy", "TwtEnergySaving"},
            )

    def test_report_never_requests_walkthrough_update(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            session_dir = generated / "sessions" / SESSION
            session_dir.mkdir(parents=True)
            (session_dir / "session.json").write_text(json.dumps({
                "suite": "ax",
                "scenario": "twt",
                "evidence": "pcap",
                "runs": 5,
                "pcap_run": 0,
                "configurations": ["BaselineEnergy"],
            }))
            args = SimpleNamespace(
                suite=str(AX_SUITE), scenario="twt", session_id=SESSION
            )
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(wifi_analysis, "run_command") as run,
            ):
                wifi_analysis.report_command(args)
            command = run.call_args.args[0]
            self.assertIn("--reuse", command)
            self.assertNotIn("--update-walkthrough", command)

    def test_scalar_report_passes_filtered_manifest_to_evaluator(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            session_dir = generated / "sessions" / SESSION
            session_dir.mkdir(parents=True)
            filtered = session_dir / "scalar-vector-manifest.json"
            filtered.write_text("{}")
            (session_dir / "session.json").write_text(json.dumps({
                "suite": "ax",
                "scenario": "twt",
                "evidence": "scalar-vector",
                "runs": 1,
                "pcap_run": None,
                "configurations": ["BaselineEnergy"],
                "scalar_vector_manifest": str(filtered),
            }))
            args = SimpleNamespace(
                suite=str(AX_SUITE), scenario="twt", session_id=SESSION
            )
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(wifi_analysis, "run_command") as run,
            ):
                wifi_analysis.report_command(args)
            evaluator = run.call_args_list[2].args[0]
            self.assertEqual(
                evaluator[evaluator.index("--manifest") + 1], str(filtered)
            )

    def test_publish_requires_update_and_five_runs(self):
        args = SimpleNamespace(
            suite=str(AX_SUITE),
            scenario="twt",
            session_id=SESSION,
            update=False,
        )
        with self.assertRaisesRegex(wifi_analysis.CliError, "requires explicit"):
            wifi_analysis.publish_command(args)

        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            session_dir = generated / "sessions" / SESSION
            session_dir.mkdir(parents=True)
            (session_dir / "session.json").write_text(json.dumps({
                "suite": "ax",
                "scenario": "twt",
                "evidence": "pcap",
                "runs": 1,
                "pcap_run": 0,
                "configurations": None,
            }))
            args.update = True
            with patch.object(wifi_analysis, "GENERATED_ROOT", generated):
                with self.assertRaisesRegex(
                    wifi_analysis.CliError, "publication requires at least 5"
                ):
                    wifi_analysis.publish_command(args)

    def test_pcap_only_session_cannot_pass_publication_gate(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            session_dir = generated / "sessions" / SESSION
            session_dir.mkdir(parents=True)
            (session_dir / "session.json").write_text(json.dumps({
                "session_id": SESSION,
                "suite": "ax",
                "scenario": "twt",
                "evidence": "pcap",
                "runs": 5,
                "pcap_run": 0,
                "configurations": ["BaselineEnergy"],
            }))
            args = SimpleNamespace(
                suite=str(AX_SUITE),
                scenario="twt",
                session_id=SESSION,
                update=True,
            )
            with patch.object(wifi_analysis, "GENERATED_ROOT", generated):
                with self.assertRaisesRegex(
                    wifi_analysis.CliError, "PCAP-only sessions"
                ):
                    wifi_analysis.publish_command(args)

    def test_publish_rejects_scalar_report_from_another_session(self):
        logical = {"session_id": SESSION}
        with tempfile.TemporaryDirectory() as directory:
            analysis = Path(directory)
            manifest = analysis / "experiments.json"
            manifest.write_text("{}")
            (analysis / "metrics.json").write_text(json.dumps({
                "_provenance": {
                    "groups": {
                        "twt": {"session_id": "20260726T130000Z"}
                    }
                }
            }))
            with self.assertRaisesRegex(
                wifi_analysis.CliError, "belongs to session"
            ):
                wifi_analysis.validate_scalar_report_session(
                    logical, manifest, "twt"
                )

    def test_unknown_and_unmapped_scenarios_are_diagnostic(self):
        with self.assertRaisesRegex(wifi_analysis.CliError, "Unknown scenario"):
            wifi_analysis.scenario_record(AX_SUITE, "not-a-scenario")
        suite, scenario = wifi_analysis.scenario_record(
            BE_SUITE, "eht_features"
        )
        with self.assertRaisesRegex(
            wifi_analysis.CliError, "no scalar/vector analysis mapping"
        ):
            wifi_analysis.scalar_mapping(suite, "eht_features", scenario)


if __name__ == "__main__":
    unittest.main()
