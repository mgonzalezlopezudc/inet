import unittest

from inet_wifi_analysis.walkthrough import (
    normalize_heading_label,
    update_script_results_session,
)


class HeadingLabelTest(unittest.TestCase):

    def test_normalizes_ownership_and_markdown_formatting(self):
        self.assertEqual(
            normalize_heading_label(
                "[agent] **Scalar and vector analysis**"
            ),
            "scalar and vector analysis",
        )


class ResultsSessionMetadataTest(unittest.TestCase):

    def test_inserts_ledger_below_title(self):
        updated = update_script_results_session(
            "# Walkthrough\n\nIntroduction.\n",
            "PCAP",
            "20260726T160000Z",
        )
        self.assertLess(
            updated.index("<!-- BEGIN SCRIPT RESULTS SESSIONS -->"),
            updated.index("Introduction."),
        )
        self.assertIn("- Scalar/vector: `NOT RUN`", updated)
        self.assertIn("- PCAP: `20260726T160000Z`", updated)
        self.assertIn(
            "`[agent]` results sessions: `NOT RECORDED`.",
            updated,
        )

    def test_updates_one_family_and_preserves_agent_metadata(self):
        original = (
            "# Walkthrough\n\n"
            "<!-- BEGIN SCRIPT RESULTS SESSIONS -->\n"
            "`[script]` results sessions:\n"
            "\n"
            "- Scalar/vector: `20260725T120411Z`\n"
            "- PCAP: `20260725T230151Z`\n"
            "<!-- END SCRIPT RESULTS SESSIONS -->\n\n"
            "`[agent]` results sessions: `20260725T120411Z`.\n"
        )
        updated = update_script_results_session(
            original,
            "PCAP",
            "20260726T160000Z",
        )
        self.assertIn("- Scalar/vector: `20260725T120411Z`", updated)
        self.assertIn("- PCAP: `20260726T160000Z`", updated)
        self.assertIn(
            "`[agent]` results sessions: `20260725T120411Z`.",
            updated,
        )

    def test_rejects_invalid_session_id(self):
        with self.assertRaisesRegex(ValueError, "Invalid results session ID"):
            update_script_results_session(
                "# Walkthrough\n",
                "PCAP",
                "latest",
            )

    def test_rejects_malformed_other_family_instead_of_erasing_it(self):
        original = (
            "# Walkthrough\n\n"
            "<!-- BEGIN SCRIPT RESULTS SESSIONS -->\n"
            "`[script]` results sessions:\n\n"
            "- Scalar/vector sessions: `20260725T120411Z`\n"
            "- PCAP: `20260725T230151Z`\n"
            "<!-- END SCRIPT RESULTS SESSIONS -->\n"
        )
        with self.assertRaisesRegex(ValueError, "Scalar/vector entry"):
            update_script_results_session(
                original,
                "PCAP",
                "20260726T160000Z",
            )

    def test_first_script_block_preserves_existing_agent_line(self):
        original = (
            "# Walkthrough\n\n"
            "`[agent]` results sessions: `20260725T120411Z`.\n\n"
            "Introduction.\n"
        )
        updated = update_script_results_session(
            original,
            "PCAP",
            "20260726T160000Z",
        )
        self.assertEqual(
            updated.count("`[agent]` results sessions:"),
            1,
        )
        self.assertIn(
            "`[agent]` results sessions: `20260725T120411Z`.",
            updated,
        )

    def test_rejects_dangling_end_marker(self):
        original = (
            "# Walkthrough\n\n"
            "<!-- END SCRIPT RESULTS SESSIONS -->\n"
        )
        with self.assertRaisesRegex(
            ValueError,
            "Malformed script results-session markers",
        ):
            update_script_results_session(
                original,
                "PCAP",
                "20260726T160000Z",
            )
