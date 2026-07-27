#!/usr/bin/env python3

import json
import math
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))

from analysis_core import (
    Condition,
    MeasurementWindow,
    atomic_write_text,
    crop_vector,
    jain,
    per_run_delay_percentile,
    resolve_session_id,
    resolve_manifest_sessions,
    summarize_ci95,
    time_weighted_integral,
    validate_disjoint_streams,
    validate_evidence_contracts,
    validate_result_layout,
    validate_unpunctured_ru,
)
from inet_wifi_analysis import (
    result_configuration_directory,
    result_root,
    result_session_directory,
)
from run_campaign import (
    available_cpu_count,
    collect_jobs,
    positive_int,
    run_jobs,
    session_id,
)
import summarize_results


class AnalysisCoreTest(unittest.TestCase):
    def test_condition_accepts_iteration_suffixes_in_result_filenames(self):
        with tempfile.TemporaryDirectory() as directory:
            result_dir = Path(directory)
            for run in range(2):
                stem = f"AxUl-numStations=8-#{run}"
                (result_dir / f"{stem}.sca").touch()
                (result_dir / f"{stem}.vec").touch()
            condition = Condition(
                group="dense_iot",
                label="AX UL",
                config="AxUl",
                ini=Path("omnetpp.ini"),
                result_dir=result_dir,
                expected_repetitions=2,
                measurement=MeasurementWindow(20, 120),
            )
            self.assertEqual(
                [pair.run_number for pair in condition.result_files],
                [0, 1],
            )

    def test_crop_vector_uses_half_open_measurement_window(self):
        window = MeasurementWindow(1, 3)
        times, values = crop_vector([0, 1, 2, 3, 4], [10, 11, 12, 13, 14], window)
        self.assertEqual(times.tolist(), [1, 2])
        self.assertEqual(values.tolist(), [11, 12])

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

    def test_delay_uses_manifest_sink_pattern(self):
        condition = unittest.mock.Mock()
        condition.config = "ScheduledOnly"
        condition.condition_metadata = {
            "sink_module_regex": r"\.server\.app\["
        }
        condition.measurement = MeasurementWindow(0.3, 2.0)
        condition.vectors.return_value = __import__("pandas").DataFrame([
            {
                "runID": "run-0",
                "module": "Network.server.app[0]",
                "vectime": [0.2, 0.4, 1.0],
                "vecvalue": [9.0, 0.001, 0.003],
            },
            {
                "runID": "run-0",
                "module": "Network.host[0].app[0]",
                "vectime": [0.4],
                "vecvalue": [1.0],
            },
        ])

        result = per_run_delay_percentile(condition, 95)

        self.assertEqual(result.runID.tolist(), ["run-0"])
        self.assertAlmostEqual(result.delay_s.iloc[0], 0.0029)

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

    def test_summarizer_accepts_manifest_and_group(self):
        manifest_path = Path("/tmp/scenario-manifest.json")
        manifest = {
            "groups": {"sample": {}},
            "evidence_contracts": {"sample": []},
        }
        with (
            patch.object(
                sys,
                "argv",
                [
                    "summarize_results.py",
                    "--manifest",
                    str(manifest_path),
                    "--group",
                    "sample",
                    "--session-id",
                    "20260725T120000Z",
                ],
            ),
            patch.object(
                summarize_results,
                "load_manifest",
                return_value=manifest,
            ) as load_manifest,
            patch.object(
                summarize_results,
                "resolve_manifest_sessions",
                return_value=({}, {"sample": "not run"}),
            ) as resolve_sessions,
            patch.object(
                summarize_results,
                "atomic_write_text",
            ) as atomic_write,
        ):
            summarize_results.main()

        load_manifest.assert_called_once_with(manifest_path)
        self.assertEqual(
            set(resolve_sessions.call_args.args[0]["groups"]), {"sample"}
        )
        payload = json.loads(atomic_write.call_args.args[1])
        self.assertEqual(
            payload["_provenance"]["requested_session_id"],
            "20260725T120000Z",
        )
        self.assertEqual(
            payload["_provenance"]["groups"]["sample"]["status"],
            "NOT RUN",
        )


class CampaignRunnerTest(unittest.TestCase):
    SESSION_ID = "20260725T120000Z"
    MANIFEST = {
        "groups": {
            "sample": {
                "ini": "sample/omnetpp.ini",
                "expected_repetitions": 2,
                "conditions": [
                    {"config": "First"},
                    {"config": "Second", "ini": "sample/alternate.ini"},
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
            f"sample/results/{self.SESSION_ID}/Second",
            result_argument,
        )

    def test_canonical_result_paths_are_derived_from_the_ini_directory(self):
        root = Path("/repository")
        ini = Path("examples/sample/omnetpp.ini")
        self.assertEqual(
            result_root(root, ini),
            root / "examples/sample/results",
        )
        self.assertEqual(
            result_session_directory(root, ini, self.SESSION_ID),
            root / "examples/sample/results" / self.SESSION_ID,
        )
        self.assertEqual(
            result_configuration_directory(
                root, ini, self.SESSION_ID, "First"
            ),
            root / "examples/sample/results" / self.SESSION_ID / "First",
        )
        absolute_ini = Path("/external/example/omnetpp.ini")
        self.assertEqual(
            result_root(root, absolute_ini),
            Path("/external/example/results"),
        )

    def test_manifest_rejects_result_root_overrides_and_split_examples(self):
        with self.assertRaisesRegex(RuntimeError, "result_dir is obsolete"):
            validate_result_layout({
                "groups": {
                    "sample": {
                        "ini": "sample/omnetpp.ini",
                        "result_dir": "elsewhere",
                        "conditions": [],
                    }
                }
            })
        with self.assertRaisesRegex(RuntimeError, "one simulation example"):
            validate_result_layout({
                "groups": {
                    "sample": {
                        "ini": "sample/omnetpp.ini",
                        "conditions": [{
                            "config": "Second",
                            "ini": "other/omnetpp.ini",
                        }],
                    }
                }
            })

    def test_campaign_records_pcap_only_for_selected_run(self):
        jobs = collect_jobs(
            self.MANIFEST,
            "sample",
            campaign_session_id=self.SESSION_ID,
            pcap_run=0,
            pcap_interface_patterns=("**.wlan[*]",),
        )
        first = next(job for job in jobs if job.config == "First" and job.run == 0)
        second = next(job for job in jobs if job.config == "First" and job.run == 1)
        self.assertIn("--**.wlan[*].recordPcap=true", first.command)
        self.assertFalse(
            any("recordPcap=true" in argument for argument in second.command)
        )

    def test_campaign_rejects_configurations_from_different_examples(self):
        manifest = {
            "groups": {
                "sample": {
                    "ini": "sample/omnetpp.ini",
                    "expected_repetitions": 1,
                    "conditions": [
                        {"config": "First"},
                        {"config": "Second", "ini": "other/omnetpp.ini"},
                    ],
                }
            }
        }
        with self.assertRaisesRegex(ValueError, "one simulation example"):
            collect_jobs(
                manifest,
                "sample",
                campaign_session_id=self.SESSION_ID,
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
                    result_dir = (
                        root
                        / "sample/results"
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
