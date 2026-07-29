import unittest
from pathlib import Path

from render_walkthrough_results import (
    independent_runs_summary,
    metric_rows,
    render_evidence_markdown,
    replace_generated_section,
    update_walkthrough,
    validate_bundle_provenance,
)


class MetricRowsTest(unittest.TestCase):

    def test_renders_run_summary_and_direct_value(self):
        rows = metric_rows({
            "Treatment": {
                "goodput_mbps": {"count": 5, "mean": 12.5, "ci95": 0.75},
                "observed_masks": [0, 2],
            }
        })
        self.assertEqual(
            rows[0],
            ["Treatment", "goodput mbps", "5", "12.5", "0.75"],
        )
        self.assertEqual(rows[1][2:], ["—", "[0, 2]", "—"])

    def test_summarizes_common_run_counts_and_direct_values(self):
        self.assertEqual(
            independent_runs_summary({
                "Treatment": {
                    "goodput": {"count": 5, "mean": 1},
                    "masks": [0, 2],
                }
            }),
            "run-level summaries: n=5; direct observations: no independent-run estimate",
        )


class EvidenceMarkdownTest(unittest.TestCase):

    def test_renders_executable_check_and_joined_scheduler_inputs(self):
        markdown = render_evidence_markdown(
            "ul_ofdma",
            {
                "session_id": "20260729T120000Z",
                "checks": [{
                    "handler": "ul_trigger_allocation_join",
                    "status": "PASS",
                    "requirement": "Trigger AID/RU join",
                    "reason": "one-to-one",
                    "observations": [{
                        "config": "BacklogBased",
                        "simulation_time": "0.5",
                        "trigger_id": 17,
                        "user_ordinal": 0,
                        "association_id": 3,
                        "reported_bytes": 1000,
                        "planned_bytes": 800,
                        "model_ru_tone_size": 106,
                        "model_ru_tone_offset": 0,
                        "pcap_ru_allocation": 53,
                        "pcap_ru_tone_size": 106,
                        "pcap_ru_tone_offset": 0,
                        "matched": True,
                    }],
                }],
            },
            "20260729T120000Z",
        )
        self.assertIn("Trigger AID/RU join", markdown)
        self.assertIn("1000 / 800", markdown)
        self.assertIn("53 → 106@0", markdown)


class GeneratedSectionTest(unittest.TestCase):

    def test_inserts_before_next_level_two_heading(self):
        original = (
            "# Walkthrough\n\n"
            "## [agent] **Scalar and vector analysis**\n\nAuthored.\n\n"
            "## [agent] PCAP statistics\n\nPacket prose.\n"
        )
        updated = replace_generated_section(
            original, "ieee80211-scalar-vector-sample", "table"
        )
        self.assertLess(
            updated.index("table"),
            updated.index("## [agent] PCAP statistics"),
        )
        self.assertIn("Authored.", updated)

    def test_marker_update_preserves_authored_text(self):
        marker = "ieee80211-scalar-vector-sample"
        original = (
            "## [agent] Scalar and vector analysis\n\nBefore.\n\n"
            f"<!-- BEGIN GENERATED: {marker} -->\nold\n"
            f"<!-- END GENERATED: {marker} -->\n\nAfter.\n"
        )
        updated = replace_generated_section(original, marker, "new")
        self.assertIn("Before.", updated)
        self.assertIn("After.", updated)
        self.assertNotIn("\nold\n", updated)
        self.assertIn("\nnew\n", updated)

    def test_update_refreshes_scalar_session_and_preserves_agent_session(self):
        content = (
            "# Walkthrough\n\n"
            "<!-- BEGIN SCRIPT RESULTS SESSIONS -->\n"
            "`[script]` results sessions:\n\n"
            "- Scalar/vector: `20260725T120411Z`\n"
            "- PCAP: `20260725T230151Z`\n"
            "<!-- END SCRIPT RESULTS SESSIONS -->\n\n"
            "`[agent]` results sessions: `20260725T120411Z`.\n\n"
            "## [agent] Scalar and vector analysis\n"
        )
        updated = update_walkthrough(
            content,
            "ieee80211-scalar-vector-sample",
            "### [script] Generated table",
            "20260726T160000Z",
        )
        self.assertIn("- Scalar/vector: `20260726T160000Z`", updated)
        self.assertIn("- PCAP: `20260725T230151Z`", updated)
        self.assertIn(
            "`[agent]` results sessions: `20260725T120411Z`.",
            updated,
        )


class ProvenanceTest(unittest.TestCase):

    def test_accepts_matching_metric_and_figure_sessions(self):
        condition = {
            "group": "width",
            "configuration": "Width20MHz",
        }
        metrics = {
            "_provenance": {
                "groups": {
                    "width": {
                        "status": "PASS",
                        "session_id": "20260725T120411Z",
                        "conditions": [condition],
                    }
                }
            }
        }
        sidecar = {"conditions": [condition]}
        self.assertEqual(
            validate_bundle_provenance(
                "width",
                metrics,
                sidecar,
                Path("results/20260725T120411Z/width.png"),
            ),
            "20260725T120411Z",
        )

    def test_rejects_missing_or_mismatched_sessions(self):
        with self.assertRaisesRegex(RuntimeError, "no PASS session provenance"):
            validate_bundle_provenance("width", {}, {"conditions": []})
        condition = {
            "group": "width",
            "configuration": "Width20MHz",
        }
        metrics = {
            "_provenance": {
                "groups": {
                    "width": {
                        "status": "PASS",
                        "session_id": "20260725T120411Z",
                        "conditions": [condition],
                    }
                }
            }
        }
        sidecar = {"conditions": [condition]}
        with self.assertRaisesRegex(RuntimeError, "sessions differ"):
            validate_bundle_provenance(
                "width",
                metrics,
                sidecar,
                Path("results/20260724T000000Z/width.png"),
            )

    def test_rejects_different_condition_metadata(self):
        metric_condition = {
            "group": "width",
            "configuration": "Width20MHz",
        }
        figure_condition = {
            **metric_condition,
            "configuration": "Width40MHz",
        }
        metrics = {
            "_provenance": {
                "groups": {
                    "width": {
                        "status": "PASS",
                        "session_id": "20260725T120411Z",
                        "conditions": [metric_condition],
                    }
                }
            }
        }
        with self.assertRaisesRegex(RuntimeError, "condition metadata differs"):
            validate_bundle_provenance(
                "width", metrics, {"conditions": [figure_condition]}
            )


if __name__ == "__main__":
    unittest.main()
