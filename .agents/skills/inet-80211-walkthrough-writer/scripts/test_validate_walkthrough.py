import unittest

from validate_walkthrough import (
    has_markdown_table,
    has_plot_or_rationale,
    section_body,
    strip_generated_blocks,
    validate_analysis_ownership,
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

    def test_generated_analysis_is_not_treated_as_agent_content(self):
        text = (
            "Intro.\n"
            "<!-- BEGIN GENERATED: pcap -->\n"
            "| Frame | Count |\n|---|---:|\n| Data | 2 |\n"
            "![Packets](packets.png)\n"
            "<!-- END GENERATED: pcap -->\n"
            "Interpretation.\n"
        )
        authored = strip_generated_blocks(text)
        self.assertNotIn("| Data |", authored)
        self.assertNotIn("![Packets]", authored)

    def test_rejects_agent_authored_analysis_presentations(self):
        base = (
            "## [agent] Scalar and vector analysis\n\n"
            "{body}\n\n"
            "## [agent] PCAP statistics\n\nInterpretation.\n\n"
            "## [agent] Frame exchange analysis\n\nInterpretation.\n"
        )
        table = "| Metric | Value |\n|---|---:|\n| Delay | 1 |\n"
        self.assertTrue(validate_analysis_ownership(base.format(body=table)))
        self.assertTrue(validate_analysis_ownership(
            base.format(body="![Delay](delay.png)")
        ))
        self.assertTrue(validate_analysis_ownership(
            base.format(body="```sh\nopp_scavetool query results.sca\n```")
        ))

    def test_accepts_script_generated_analysis_with_agent_interpretation(self):
        text = (
            "## [agent] Scalar and vector analysis\n\n"
            "The treatment lowers delay in this scope.\n\n"
            "<!-- BEGIN GENERATED: scalar -->\n"
            "### [script] Results\n"
            "| Metric | Value |\n|---|---:|\n| Delay | 1 |\n"
            "![Delay](delay.png)\n"
            "<!-- END GENERATED: scalar -->\n\n"
            "## [agent] PCAP statistics\n\nInterpretation.\n\n"
            "## [agent] Frame exchange analysis\n\nInterpretation.\n"
        )
        self.assertEqual(validate_analysis_ownership(text), [])


if __name__ == "__main__":
    unittest.main()
