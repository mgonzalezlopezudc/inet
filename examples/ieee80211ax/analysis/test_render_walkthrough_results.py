import unittest
from pathlib import Path

from render_walkthrough_results import (
    metric_rows,
    replace_generated_section,
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


class GeneratedSectionTest(unittest.TestCase):

    def test_inserts_before_next_level_two_heading(self):
        original = (
            "# Walkthrough\n\n## **Scalar and vector analysis**\n\nAuthored.\n\n"
            "## PCAP statistics\n\nPacket prose.\n"
        )
        updated = replace_generated_section(
            original, "ieee80211-scalar-vector-sample", "table"
        )
        self.assertLess(updated.index("table"), updated.index("## PCAP statistics"))
        self.assertIn("Authored.", updated)

    def test_marker_update_preserves_authored_text(self):
        marker = "ieee80211-scalar-vector-sample"
        original = (
            "## Scalar and vector analysis\n\nBefore.\n\n"
            f"<!-- BEGIN GENERATED: {marker} -->\nold\n"
            f"<!-- END GENERATED: {marker} -->\n\nAfter.\n"
        )
        updated = replace_generated_section(original, marker, "new")
        self.assertIn("Before.", updated)
        self.assertIn("After.", updated)
        self.assertNotIn("\nold\n", updated)
        self.assertIn("\nnew\n", updated)


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
