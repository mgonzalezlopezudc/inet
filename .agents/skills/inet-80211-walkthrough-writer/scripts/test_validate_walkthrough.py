import unittest

from validate_walkthrough import (
    has_markdown_table,
    has_plot_or_rationale,
    section_body,
)


class AnalysisPresentationValidationTest(unittest.TestCase):

    def test_section_body_accepts_formatted_heading(self):
        text = (
            "## **Scalar and vector analysis**\n\nBody.\n\n"
            "## PCAP statistics\n\nPackets.\n"
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


if __name__ == "__main__":
    unittest.main()
