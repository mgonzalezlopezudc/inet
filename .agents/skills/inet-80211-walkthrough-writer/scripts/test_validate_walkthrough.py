import unittest

from validate_walkthrough import (
    has_markdown_table,
    has_plot_or_rationale,
    section_body,
    validate_heading_ownership,
    validate_session_ledger,
)


class AnalysisPresentationValidationTest(unittest.TestCase):

    def test_section_body_accepts_formatted_heading(self):
        text = (
            "## [agent] **Scalar and vector analysis**\n\nBody.\n\n"
            "## [agent] PCAP statistics\n\nPackets.\n"
        )
        self.assertIn(
            "Body.", section_body(text, "Scalar and vector analysis")
        )

    def test_detects_compact_table(self):
        self.assertTrue(has_markdown_table(
            "| Configuration | Metric |\n|---|---:|\n| A | 1 |\n"
        ))

    def test_accepts_plot_or_plain_no_plot_rationale(self):
        self.assertTrue(has_plot_or_rationale(
            "![Timeline](timeline.png)\n"
        ))
        self.assertTrue(has_plot_or_rationale(
            "No plot: a single value is clearer in the table.\n"
        ))

    def test_heading_ownership_follows_generated_boundaries(self):
        text = (
            "## [agent] PCAP statistics\n"
            "<!-- BEGIN GENERATED: pcap -->\n"
            "### [script] Generated table\n"
            "<!-- END GENERATED: pcap -->\n"
            "## [agent] Verdict\n"
        )
        self.assertEqual(validate_heading_ownership(text), [])
        self.assertTrue(validate_heading_ownership(
            text.replace("### [script]", "### [agent]")
        ))
        self.assertTrue(validate_heading_ownership(
            text.replace("## [agent] Verdict", "## Verdict")
        ))

    def test_session_ledger_accepts_separate_owners_and_families(self):
        text = (
            "# Walkthrough\n\n"
            "<!-- BEGIN SCRIPT RESULTS SESSIONS -->\n"
            "`[script]` results sessions:\n"
            "- Scalar/vector: `20260726T160000Z`\n"
            "- PCAP: `NOT RUN`\n"
            "<!-- END SCRIPT RESULTS SESSIONS -->\n\n"
            "`[agent]` results sessions: `20260725T120411Z`, "
            "`20260725T230151Z`.\n\n"
            "## [agent] Evidence status\n"
        )
        self.assertEqual(validate_session_ledger(text), [])
        self.assertTrue(validate_session_ledger(
            text.replace("- PCAP: `NOT RUN`\n", "")
        ))


if __name__ == "__main__":
    unittest.main()
