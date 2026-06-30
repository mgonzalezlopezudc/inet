import tempfile
import unittest
from pathlib import Path

try:
    from . import processor
except ImportError:
    import processor


class StandardsProcessorTest(unittest.TestCase):
    def test_detects_clause_table_figure_and_annex_headings(self):
        pages = [
            (1, "1.1 Scope\nThe scope text\nTable 27-61 HE PHY timing\nTiming text\n"),
            (2, "Figure 35-1 EHT PPDU format\nFigure text\nA.1 Annex heading\nAnnex text\n"),
        ]
        chunks = processor.chunk_pages("doc", pages)

        self.assertEqual(["clause", "table", "figure", "clause"], [chunk.kind for chunk in chunks])
        self.assertEqual("1.1", chunks[0].label)
        self.assertEqual("Table 27-61", chunks[1].label)
        self.assertEqual("Figure 35-1", chunks[2].label)
        self.assertEqual("A.1", chunks[3].label)

    def test_ignores_toc_leaders_and_license_footer(self):
        text = (
            "27.3 HE PHY ........................................................................................ 4000\n"
            "Authorized licensed use limited to: Example Licensee. Downloaded on June 30,2026 at 16:54:22 UTC from IEEE Xplore. Restrictions apply.\n"
            "27.3 HE PHY\n"
            "Useful text\n"
        )
        chunks = processor.chunk_pages("doc", [(3, text)])

        self.assertEqual(2, len(chunks))
        self.assertEqual("page", chunks[0].kind)
        self.assertEqual("clause", chunks[1].kind)
        self.assertNotIn("Authorized licensed use", chunks[1].text)

    def test_fts_query_tokenizes_hyphenated_table_numbers(self):
        self.assertEqual("Table AND 27 AND 61", processor.fts_query("Table 27-61"))

    def test_show_reads_page_identifier(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            page_dir = output / "pages" / "doc"
            page_dir.mkdir(parents=True)
            (page_dir / "page-0007.txt").write_text("page text\n", encoding="utf-8")

            item = processor.show(output_dir=output, identifier="doc:page:7")

            self.assertEqual("page", item["type"])
            self.assertEqual(7, item["page_start"])
        self.assertEqual("page text\n", item["text"])

    def test_status_reports_stale_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "manifest.json").write_text(
                """{
  "pipeline_version": "1",
  "extraction": {"pdftotext_args": ["-layout"], "page_spec": null},
  "documents": [{"doc_id": "doc", "sha256": "old"}]
}
""",
                encoding="utf-8",
            )
            original_expected = processor.expected_pdf_entries
            try:
                processor.expected_pdf_entries = lambda pdfs: [{
                    "doc_id": "doc",
                    "pdf_path": "doc.pdf",
                    "sha256": "new",
                    "pages_total": 1,
                    "title": "Doc",
                }]
                result = processor.status(output_dir=output, pdfs=["doc.pdf"])
            finally:
                processor.expected_pdf_entries = original_expected

            self.assertEqual("stale", result["documents"][0]["state"])


if __name__ == "__main__":
    unittest.main()
