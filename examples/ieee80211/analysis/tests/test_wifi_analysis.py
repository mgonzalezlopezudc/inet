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
from inet_wifi_analysis import scenario_configuration_ini


AX_SUITE = ANALYSIS_ROOT / "suites" / "ax.json"
BE_SUITE = ANALYSIS_ROOT / "suites" / "be-eht-features.json"
SESSION = "20260726T120000Z"


class WifiAnalysisCliTest(unittest.TestCase):

    def test_configuration_ini_mapping_preserves_ax_and_eht_examples(self):
        ax = wifi_analysis.load_suite(AX_SUITE, wifi_analysis.REPOSITORY_ROOT)
        multi_tid = ax.scenarios["ul_multitid"]
        self.assertIn("UlSUHTAMpduCompressedBlockAck", multi_tid["configurations"])
        self.assertIn("ht", multi_tid["phy_profiles"])
        self.assertEqual(
            scenario_configuration_ini(
                ax.example_root, multi_tid, "UlMuMultiTidBlockAck"
            ),
            ax.example_root
            / "ul_multitid/omnetpp.ini",
        )
        be = wifi_analysis.load_suite(BE_SUITE, wifi_analysis.REPOSITORY_ROOT)
        eht = be.scenarios["eht_features"]
        self.assertEqual(
            scenario_configuration_ini(
                be.example_root, eht, "EhtFeatures"
            ).parent,
            be.example_root / "eht_features",
        )

    def test_ax_mapped_groups_are_available_to_both_pipelines(self):
        suite = wifi_analysis.load_suite(
            AX_SUITE, wifi_analysis.REPOSITORY_ROOT
        )
        self.assertTrue(
            all(
                "scalar_vector_group" in scenario
                for scenario in suite.scenarios.values()
            )
        )
        for scenario_name, scenario in suite.scenarios.items():
            if "scalar_vector_group" not in scenario:
                continue
            self.assertTrue(
                (suite.example_root / scenario["ini"]).is_file(),
                scenario_name,
            )
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
                "evidence": "scalar-vector",
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
                exhaustive_vectors=True,
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
            self.assertIn("--pcap-run", commands[0])
            self.assertIn("--exhaustive-vectors", commands[0])
            self.assertEqual(
                Path(commands[1][1]),
                ANALYSIS_ROOT / "analyze_pcap.py",
            )
            self.assertIn("--index", commands[1])
            self.assertNotIn("--generate", commands[1])
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
            self.assertTrue(logical["exhaustive_vectors"])

    def test_ul_ofdma_session_records_the_evidence_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            args = SimpleNamespace(
                suite=str(AX_SUITE),
                scenario="ul_ofdma",
                evidence="scalar-vector",
                runs=5,
                config=["BacklogBased5ms", "EdcaBaseline5ms"],
                jobs=2,
                session_id=SESSION,
                exhaustive_vectors=True,
            )
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(wifi_analysis, "run_command"),
                redirect_stdout(io.StringIO()),
            ):
                wifi_analysis.run_command_handler(args)
            logical = json.loads(
                (generated / "sessions" / SESSION / "session.json").read_text()
            )
            self.assertEqual(
                logical["recording"],
                {
                    "profile": "performance-with-run0-diagnostic",
                    "diagnostic_run": 0,
                },
            )

    def test_disabled_diagnostic_vectors_are_not_claimed_in_session(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            args = SimpleNamespace(
                suite=str(AX_SUITE),
                scenario="ul_ofdma",
                evidence="scalar-vector",
                runs=5,
                config=["BacklogBased5ms"],
                jobs=2,
                session_id=SESSION,
                exhaustive_vectors=False,
            )
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(wifi_analysis, "run_command"),
                redirect_stdout(io.StringIO()),
            ):
                wifi_analysis.run_command_handler(args)
            logical = json.loads(
                (generated / "sessions" / SESSION / "session.json").read_text()
            )
            self.assertIsNone(logical["recording"])
            self.assertFalse(logical["exhaustive_vectors"])

    def test_report_never_requests_walkthrough_update(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            session_dir = generated / "sessions" / SESSION
            session_dir.mkdir(parents=True)
            filtered = session_dir / "scalar-vector-manifest.json"
            filtered.write_text("{}")
            (session_dir / "session.json").write_text(json.dumps({
                "suite": "ax",
                "scenario": "twt",
                "evidence": "both",
                "runs": 5,
                "pcap_run": 0,
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
            pcap_command = next(
                call.args[0] for call in run.call_args_list
                if "analyze_pcap.py" in call.args[0][1]
            )
            self.assertIn("--reuse", pcap_command)
            self.assertNotIn("--update-walkthrough", pcap_command)

    def test_both_report_analyzes_pcap_before_cross_layer_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            session_dir = generated / "sessions" / SESSION
            session_dir.mkdir(parents=True)
            filtered = session_dir / "scalar-vector-manifest.json"
            filtered.write_text("{}")
            (session_dir / "session.json").write_text(json.dumps({
                "suite": "ax",
                "scenario": "ul_ofdma",
                "evidence": "both",
                "runs": 5,
                "pcap_run": 0,
                "configurations": ["BacklogBased5ms"],
                "scalar_vector_manifest": str(filtered),
            }))
            args = SimpleNamespace(
                suite=str(AX_SUITE),
                scenario="ul_ofdma",
                session_id=SESSION,
            )
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(wifi_analysis, "run_command") as run,
            ):
                wifi_analysis.report_command(args)
            scripts = [
                Path(call.args[0][1]).name for call in run.call_args_list
            ]
            self.assertEqual(
                scripts,
                [
                    "summarize_results.py",
                    "first_tranche.py",
                    "analyze_pcap.py",
                    "evaluate_evidence.py",
                ],
            )

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
            summarizer = run.call_args_list[0].args[0]
            self.assertIn("summarize_results.py", summarizer[1])
            self.assertNotIn("_ax_scalar_adapter.py", summarizer[1])
            self.assertEqual(
                summarizer[summarizer.index("--manifest") + 1], str(filtered)
            )
            self.assertEqual(
                summarizer[summarizer.index("--group") + 1], "twt"
            )
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
                "evidence": "scalar-vector",
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

    def test_legacy_pcap_only_session_is_rejected(self):
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
                    wifi_analysis.CliError, "unsupported evidence mode"
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

    def test_publish_passes_session_ledger_to_renderer(self):
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "generated"
            session_dir = generated / "sessions" / SESSION
            session_dir.mkdir(parents=True)
            filtered = session_dir / "scalar-vector-manifest.json"
            filtered.write_text("{}")
            (session_dir / "session.json").write_text(json.dumps({
                "session_id": SESSION,
                "suite": "ax",
                "scenario": "twt",
                "evidence": "scalar-vector",
                "runs": 5,
                "pcap_run": None,
                "configurations": ["BaselineEnergy"],
                "scalar_vector_manifest": str(filtered),
            }))
            args = SimpleNamespace(
                suite=str(AX_SUITE),
                scenario="twt",
                session_id=SESSION,
                update=True,
            )
            with (
                patch.object(wifi_analysis, "GENERATED_ROOT", generated),
                patch.object(
                    wifi_analysis, "validate_scalar_report_session"
                ),
                patch.object(wifi_analysis, "run_command") as run,
            ):
                wifi_analysis.publish_command(args)
            command = run.call_args.args[0]
            self.assertEqual(
                command[command.index("--evidence-ledger") + 1],
                str(session_dir / "evidence-ledger.json"),
            )

    def test_unknown_scenario_and_eht_mapping(self):
        with self.assertRaisesRegex(wifi_analysis.CliError, "Unknown scenario"):
            wifi_analysis.scenario_record(AX_SUITE, "not-a-scenario")
        suite, scenario = wifi_analysis.scenario_record(
            BE_SUITE, "eht_features"
        )
        _, group = wifi_analysis.scalar_mapping(
            suite, "eht_features", scenario
        )
        self.assertEqual(group, "eht_features")


if __name__ == "__main__":
    unittest.main()
