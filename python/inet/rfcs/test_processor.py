import tempfile
import unittest
from pathlib import Path

try:
    from . import processor
except ImportError:
    import processor


RFC_TEXT = """Internet Engineering Task Force (IETF)
Request for Comments: 9999
Category: Standards Track
July 2026

Test Protocol

Abstract
   This document is a fixture.

Table of Contents
   1.  Introduction ................................................. 1
   2.  Packet Format ................................................ 2

1.  Introduction
   The alpha behavior is normative.

Example Author [Page 1]
RFC 9999                    Test Protocol                    July 2026

2.  Packet Format
   The packet contains beta fields.

   Figure 1: Packet Layout
      +-+-+-+

Appendix A.  Notes
   Appendix text.

9.  References
   [X] Example.
"""


def fake_document(number, title="Test Protocol", errata=None, updated_by=None):
    text = RFC_TEXT.replace("9999", str(number))
    return {
        "text": text,
        "metadata": {
            "doc_id": f"rfc{number}",
            "rfc_number": number,
            "source_url": f"https://www.rfc-editor.org/rfc/rfc{number}.txt",
            "sha256": processor.sha256_text(text),
            "byte_length": len(text.encode("utf-8")),
            "fetched_at": "2026-07-01T00:00:00+00:00",
            "etag": "\"fixture\"",
            "last_modified": "Wed, 01 Jul 2026 00:00:00 GMT",
            "title": title,
            "published": "July 2026",
            "status": "Standards Track",
            "updates": [],
            "obsoletes": [],
            "updated_by": updated_by or [],
            "obsoleted_by": [],
            "errata_snapshot": errata or {"available": True, "total": 0, "ids_by_status": {}, "counts_by_status": {}},
            "accepted_errata": False,
        },
    }


class RfcProcessorTest(unittest.TestCase):
    def test_detects_sections_references_appendices_and_figures(self):
        chunks = processor.chunk_rfc_text("Test", "rfc9999", RFC_TEXT)

        kinds = [chunk.kind for chunk in chunks]
        self.assertIn("section", kinds)
        self.assertIn("figure", kinds)
        self.assertIn("appendix", kinds)
        self.assertIn("references", kinds)
        self.assertEqual("1", next(chunk.section for chunk in chunks if chunk.heading.startswith("1 Introduction")))
        self.assertFalse(any("Table of Contents" in chunk.heading for chunk in chunks))

    def test_extracts_multiline_titles_and_bounded_info_relationships(self):
        title = processor.parse_title_from_text(4443, """Network Working Group                                           A. Conta
Request for Comments: 4443                                    Transwitch
Obsoletes: 2463                                               S. Deering
Updates: 2780                                              Cisco Systems
Category: Standards Track                                  M. Gupta, Ed.
                                                              March 2006


               Internet Control Message Protocol (ICMPv6)
        for the Internet Protocol Version 6 (IPv6) Specification

Status of This Memo
""")
        metadata = processor.parse_info_metadata(8200, b"""
<h1>RFC 8200 STD 86 : Internet Protocol, Version 6 (IPv6) Specification | RFC Editor</h1>
<p>This RFC was updated, see</p><a>RFC 9673</a>
<pre>Obsoletes: 2460</pre>
<p>References</p><a>RFC 791</a><a>RFC 4291</a>
""")

        self.assertEqual("Internet Control Message Protocol (ICMPv6) for the Internet Protocol Version 6 (IPv6) Specification", title)
        self.assertEqual([9673], metadata["updated_by"])
        self.assertEqual("Internet Protocol, Version 6 (IPv6) Specification", metadata["title"])

    def test_build_search_show_and_protocol_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            catalog_path = root / "catalog.json"
            catalog_path.write_text('{"protocols": {"Alpha": {"rfcs": [9999]}, "Beta": {"rfcs": [9998]}}}\n', encoding="utf-8")
            original_fetch = processor.fetch_rfc_document
            try:
                processor.fetch_rfc_document = lambda number, accept_errata=False: fake_document(number)
                processor.build(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha")
                processor.build(rfcs_dir=root, catalog_path=catalog_path, protocol="Beta")

                alpha_rows = processor.search(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha", query="alpha normative")
                all_rows = processor.search(rfcs_dir=root, catalog_path=catalog_path, all_protocols=True, query="beta fields", limit=10)
                item = processor.show(rfcs_dir=root, catalog_path=catalog_path, identifier=alpha_rows[0]["chunk_id"], protocol="Alpha")
            finally:
                processor.fetch_rfc_document = original_fetch

            self.assertEqual({"Alpha"}, {row["protocol"] for row in alpha_rows})
            self.assertTrue(any(row["protocol"] == "Beta" for row in all_rows))
            self.assertEqual("chunk", item["type"])
            self.assertIn("alpha behavior", item["text"])

    def test_status_reports_freshness_variants(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            catalog_path = root / "catalog.json"
            catalog_path.write_text('{"protocols": {"Alpha": {"rfcs": [9999]}}}\n', encoding="utf-8")
            original_fetch = processor.fetch_rfc_document
            try:
                processor.fetch_rfc_document = lambda number, accept_errata=False: fake_document(number)
                processor.build(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha")
                fresh = processor.status(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha")

                processor.fetch_rfc_document = lambda number, accept_errata=False: fake_document(number, title="New Title")
                metadata_stale = processor.status(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha")

                processor.fetch_rfc_document = lambda number, accept_errata=False: fake_document(number, errata={
                    "available": True,
                    "total": 1,
                    "ids_by_status": {"Verified": [1]},
                    "counts_by_status": {"Verified": 1},
                })
                errata_stale = processor.status(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha")

                processor.fetch_rfc_document = lambda number, accept_errata=False: fake_document(number, updated_by=[10000])
                relationship_stale = processor.status(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha")
            finally:
                processor.fetch_rfc_document = original_fetch

            self.assertEqual("fresh", fresh["protocols"][0]["state"])
            self.assertEqual("metadata-stale", metadata_stale["protocols"][0]["state"])
            self.assertEqual("errata-stale", errata_stale["protocols"][0]["state"])
            self.assertEqual("metadata-stale", relationship_stale["protocols"][0]["state"])

    def test_offline_status_detects_catalog_change(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            catalog_path = root / "catalog.json"
            catalog_path.write_text('{"protocols": {"Alpha": {"rfcs": [9999]}}}\n', encoding="utf-8")
            original_fetch = processor.fetch_rfc_document
            try:
                processor.fetch_rfc_document = lambda number, accept_errata=False: fake_document(number)
                processor.build(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha")
            finally:
                processor.fetch_rfc_document = original_fetch

            catalog_path.write_text('{"protocols": {"Alpha": {"rfcs": [9999, 10000]}}}\n', encoding="utf-8")
            result = processor.status(rfcs_dir=root, catalog_path=catalog_path, protocol="Alpha", offline=True)

            self.assertEqual("stale", result["protocols"][0]["state"])


if __name__ == "__main__":
    unittest.main()
