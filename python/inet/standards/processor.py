import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import sqlite3
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


PIPELINE_VERSION = "2"
DEFAULT_STANDARDS_DIR = Path("standards")
DEFAULT_OUTPUT_DIR = DEFAULT_STANDARDS_DIR / "processed"
PDFTOTEXT_ARGS = ["-layout"]
LICENSE_FOOTER_RE = re.compile(
    r"^\s*Authorized licensed use limited to: .* Downloaded on .* IEEE Xplore\. Restrictions apply\.\s*$"
)
TABLE_FIGURE_RE = re.compile(r"^\s*(Table|Figure)\s+([A-Z]?\d+(?:-\d+)?|[A-Z]-\d+)\.?\s*[-\u2013\u2014.]?\s*(.{0,180})\s*$")
NUMBERED_CLAUSE_RE = re.compile(r"^\s*(\d{1,2}(?:\.\d+){1,8})\s+([A-Za-z][^.\n]{2,180})\s*$")
ANNEX_CLAUSE_RE = re.compile(r"^\s*([A-Z](?:\.\d+){1,8})\s+([A-Za-z][^.\n]{2,180})\s*$")
TOC_LEADER_RE = re.compile(r"\.{4,}\s*\d+\s*$")


@dataclass
class Heading:
    kind: str
    label: str
    heading: str


@dataclass
class Chunk:
    doc_id: str
    chunk_id: str
    kind: str
    label: str
    heading: str
    page_start: int
    page_end: int
    text: str

    def to_json(self):
        return {
            "doc_id": self.doc_id,
            "chunk_id": self.chunk_id,
            "kind": self.kind,
            "label": self.label,
            "heading": self.heading,
            "page_start": self.page_start,
            "page_end": self.page_end,
            "text": self.text,
        }


def discover_pdfs(standards_dir):
    return sorted(Path(standards_dir).glob("*.pdf"))


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run_command(args):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def command_text(args):
    completed = run_command(args)
    return (completed.stdout + completed.stderr).decode("utf-8", errors="replace").strip()


def tool_version(name):
    try:
        first_line = command_text([name, "-v"]).splitlines()[0]
        return first_line
    except Exception as error:
        return f"unavailable: {error}"


def pdfinfo(path):
    info = {}
    text = command_text(["pdfinfo", str(path)])
    for line in text.splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        info[key.strip()] = value.strip()
    return info


def parse_page_spec(page_spec):
    if not page_spec:
        return None
    pages = []
    for part in page_spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start_text, end_text = part.split("-", 1)
            start = int(start_text)
            end = int(end_text)
            if start <= 0 or end < start:
                raise ValueError(f"Invalid page range: {part}")
            pages.extend(range(start, end + 1))
        else:
            page = int(part)
            if page <= 0:
                raise ValueError(f"Invalid page number: {part}")
            pages.append(page)
    return sorted(dict.fromkeys(pages))


def contiguous_ranges(values):
    if not values:
        return []
    ranges = []
    start = previous = values[0]
    for value in values[1:]:
        if value == previous + 1:
            previous = value
            continue
        ranges.append((start, previous))
        start = previous = value
    ranges.append((start, previous))
    return ranges


def normalize_text(text):
    return text.replace("\r\n", "\n").replace("\r", "\n")


def extract_pdf_text(pdf_path, pages=None):
    if pages is None:
        completed = run_command(["pdftotext", *PDFTOTEXT_ARGS, str(pdf_path), "-"])
        return normalize_text(completed.stdout.decode("utf-8", errors="replace")), None

    segments = []
    for start, end in contiguous_ranges(pages):
        completed = run_command(["pdftotext", *PDFTOTEXT_ARGS, "-f", str(start), "-l", str(end), str(pdf_path), "-"])
        segment = normalize_text(completed.stdout.decode("utf-8", errors="replace")).rstrip("\f")
        segments.append(segment)
    return "\f".join(segments), pages


def split_pages(text):
    pages = text.split("\f")
    if pages and pages[-1] == "":
        pages = pages[:-1]
    return pages


def clean_page_text(text):
    lines = []
    for line in normalize_text(text).splitlines():
        if is_license_footer_line(line):
            continue
        lines.append(line.rstrip())
    return "\n".join(lines).strip()


def is_license_footer_line(line):
    stripped = line.strip()
    if LICENSE_FOOTER_RE.match(stripped):
        return True
    footer_fragments = [
        "Authorized licensed use limited to:",
        "IEEE Xplore. Restrictions apply.",
        "Downloaded onrights reserved.",
    ]
    if any(fragment in stripped for fragment in footer_fragments):
        return True
    if re.search(r"©\s+\d{4}\s+IEEE\. All", stripped):
        return True
    if re.search(r"Downloaded on .* IEEE Xplore", stripped):
        return True
    return stripped == "Copyright"


def is_probable_toc_line(line):
    return bool(TOC_LEADER_RE.search(line)) or line.count(".") > 12


def detect_heading(line):
    stripped = line.strip()
    if len(stripped) < 5 or is_probable_toc_line(stripped):
        return None

    match = TABLE_FIGURE_RE.match(stripped)
    if match:
        kind = match.group(1).lower()
        label = f"{match.group(1)} {match.group(2)}"
        title = match.group(3).strip()
        heading = f"{label} {title}".strip()
        return Heading(kind=kind, label=label, heading=heading)

    match = NUMBERED_CLAUSE_RE.match(stripped)
    if match:
        label = match.group(1)
        title = match.group(2).strip()
        return Heading(kind="clause", label=label, heading=f"{label} {title}")

    match = ANNEX_CLAUSE_RE.match(stripped)
    if match:
        label = match.group(1)
        title = match.group(2).strip()
        return Heading(kind="clause", label=label, heading=f"{label} {title}")

    return None


def make_page_numbers(page_count, requested_pages):
    if requested_pages is not None:
        return requested_pages
    return list(range(1, page_count + 1))


def chunk_pages(doc_id, pages):
    chunks = []
    current = None
    current_lines = []
    current_start = None
    current_end = None

    def flush():
        nonlocal current, current_lines, current_start, current_end
        if current is None or current_start is None:
            current = None
            current_lines = []
            return
        text = "\n".join(current_lines).strip()
        if text:
            chunk_id = f"{doc_id}:chunk:{len(chunks) + 1:05d}"
            chunks.append(Chunk(
                doc_id=doc_id,
                chunk_id=chunk_id,
                kind=current.kind,
                label=current.label,
                heading=current.heading,
                page_start=current_start,
                page_end=current_end,
                text=text,
            ))
        current = None
        current_lines = []
        current_start = None
        current_end = None

    for page_number, page_text in pages:
        cleaned = clean_page_text(page_text)
        if not cleaned:
            continue
        if current is None:
            current = Heading(kind="page", label=f"Page {page_number}", heading=f"Page {page_number}")
            current_start = page_number
        current_end = page_number

        for line in cleaned.splitlines():
            heading = detect_heading(line)
            if heading:
                flush()
                current = heading
                current_start = page_number
                current_end = page_number
            current_lines.append(line)

    flush()
    return chunks


def ensure_fts5_available():
    connection = sqlite3.connect(":memory:")
    try:
        connection.execute("CREATE VIRTUAL TABLE x USING fts5(body)")
    finally:
        connection.close()


def prepare_output_dirs(output_dir):
    output_dir = Path(output_dir)
    for subdir in ["text", "pages", "chunks"]:
        (output_dir / subdir).mkdir(parents=True, exist_ok=True)
    return output_dir


def write_doc_artifacts(output_dir, doc_id, text, page_numbers):
    output_dir = Path(output_dir)
    text_path = output_dir / "text" / f"{doc_id}.txt"
    text_path.write_text(text, encoding="utf-8")

    pages = split_pages(text)
    if len(pages) != len(page_numbers):
        raise RuntimeError(f"{doc_id}: extracted {len(pages)} pages, expected {len(page_numbers)}")

    page_dir = output_dir / "pages" / doc_id
    if page_dir.exists():
        shutil.rmtree(page_dir)
    page_dir.mkdir(parents=True)
    numbered_pages = list(zip(page_numbers, pages))
    for page_number, page_text in numbered_pages:
        (page_dir / f"page-{page_number:04d}.txt").write_text(page_text.strip() + "\n", encoding="utf-8")

    chunks = chunk_pages(doc_id, numbered_pages)
    chunk_path = output_dir / "chunks" / f"{doc_id}.jsonl"
    with chunk_path.open("w", encoding="utf-8") as stream:
        for chunk in chunks:
            stream.write(json.dumps(chunk.to_json(), ensure_ascii=False) + "\n")

    return {
        "text": str(text_path),
        "pages": str(page_dir),
        "chunks": str(chunk_path),
        "chunk_count": len(chunks),
        "page_count": len(numbered_pages),
    }, chunks


def build_sqlite_index(output_dir, chunks_by_doc, documents):
    ensure_fts5_available()
    index_path = Path(output_dir) / "index.sqlite"
    if index_path.exists():
        index_path.unlink()
    connection = sqlite3.connect(index_path)
    try:
        connection.execute("CREATE TABLE documents (doc_id TEXT PRIMARY KEY, pdf_path TEXT, sha256 TEXT, title TEXT, pages INTEGER)")
        connection.execute(
            "CREATE TABLE chunks ("
            "doc_id TEXT NOT NULL, chunk_id TEXT PRIMARY KEY, kind TEXT NOT NULL, label TEXT, "
            "heading TEXT, page_start INTEGER NOT NULL, page_end INTEGER NOT NULL, text TEXT NOT NULL)"
        )
        connection.execute("CREATE VIRTUAL TABLE chunks_fts USING fts5(heading, text, content='chunks', content_rowid='rowid')")
        for document in documents:
            connection.execute(
                "INSERT INTO documents(doc_id, pdf_path, sha256, title, pages) VALUES (?, ?, ?, ?, ?)",
                (document["doc_id"], document["pdf_path"], document["sha256"], document.get("title", ""), document.get("pages_total", 0)),
            )
        for chunks in chunks_by_doc.values():
            for chunk in chunks:
                cursor = connection.execute(
                    "INSERT INTO chunks(doc_id, chunk_id, kind, label, heading, page_start, page_end, text) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                    (chunk.doc_id, chunk.chunk_id, chunk.kind, chunk.label, chunk.heading, chunk.page_start, chunk.page_end, chunk.text),
                )
                connection.execute(
                    "INSERT INTO chunks_fts(rowid, heading, text) VALUES (?, ?, ?)",
                    (cursor.lastrowid, chunk.heading, chunk.text),
                )
        connection.commit()
    finally:
        connection.close()
    return index_path


def write_manifest(output_dir, documents, page_spec):
    manifest = {
        "pipeline_version": PIPELINE_VERSION,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "python_version": platform.python_version(),
        "tool_versions": {
            "pdftotext": tool_version("pdftotext"),
            "pdfinfo": tool_version("pdfinfo"),
        },
        "extraction": {
            "pdftotext_args": PDFTOTEXT_ARGS,
            "page_spec": page_spec,
        },
        "documents": documents,
    }
    manifest_path = Path(output_dir) / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def load_manifest(output_dir):
    path = Path(output_dir) / "manifest.json"
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def expected_pdf_entries(pdf_paths):
    entries = []
    for pdf_path in pdf_paths:
        info = pdfinfo(pdf_path)
        entries.append({
            "doc_id": pdf_path.stem,
            "pdf_path": str(pdf_path),
            "sha256": sha256_file(pdf_path),
            "pages_total": int(info.get("Pages", "0")),
            "title": info.get("Title", ""),
        })
    return entries


def artifacts_exist(output_dir, document):
    doc_id = document["doc_id"]
    return (
        (Path(output_dir) / "text" / f"{doc_id}.txt").exists()
        and (Path(output_dir) / "pages" / doc_id).is_dir()
        and (Path(output_dir) / "chunks" / f"{doc_id}.jsonl").exists()
        and (Path(output_dir) / "index.sqlite").exists()
    )


def is_fresh(output_dir, manifest, expected_documents, page_spec):
    if not manifest:
        return False
    if manifest.get("pipeline_version") != PIPELINE_VERSION:
        return False
    if manifest.get("extraction", {}).get("pdftotext_args") != PDFTOTEXT_ARGS:
        return False
    if manifest.get("extraction", {}).get("page_spec") != page_spec:
        return False
    actual = {document["doc_id"]: document for document in manifest.get("documents", [])}
    expected = {document["doc_id"]: document for document in expected_documents}
    if set(actual) != set(expected):
        return False
    for doc_id, expected_document in expected.items():
        actual_document = actual[doc_id]
        if actual_document.get("sha256") != expected_document.get("sha256"):
            return False
        if not artifacts_exist(output_dir, actual_document):
            return False
    return True


def build(output_dir=DEFAULT_OUTPUT_DIR, standards_dir=DEFAULT_STANDARDS_DIR, pdfs=None, page_spec=None, force=False):
    pdf_paths = [Path(pdf) for pdf in pdfs] if pdfs else discover_pdfs(standards_dir)
    if not pdf_paths:
        raise RuntimeError(f"No PDF files found in {standards_dir}")

    requested_pages = parse_page_spec(page_spec)
    expected_documents = expected_pdf_entries(pdf_paths)
    manifest = load_manifest(output_dir)
    if not force and is_fresh(output_dir, manifest, expected_documents, page_spec):
        return {"status": "fresh", "documents": manifest["documents"], "output_dir": str(output_dir)}

    output_dir = prepare_output_dirs(output_dir)
    documents = []
    chunks_by_doc = {}
    for pdf_path, expected in zip(pdf_paths, expected_documents):
        doc_id = expected["doc_id"]
        text, extracted_pages = extract_pdf_text(pdf_path, requested_pages)
        pages = split_pages(text)
        page_numbers = make_page_numbers(len(pages), extracted_pages)
        artifacts, chunks = write_doc_artifacts(output_dir, doc_id, text, page_numbers)
        document = dict(expected)
        document.update({
            "pdf_path": str(pdf_path),
            "pages_generated": len(page_numbers),
            "artifacts": artifacts,
        })
        documents.append(document)
        chunks_by_doc[doc_id] = chunks

    build_sqlite_index(output_dir, chunks_by_doc, documents)
    write_manifest(output_dir, documents, page_spec)
    return {"status": "built", "documents": documents, "output_dir": str(output_dir)}


def fts_query(user_query):
    tokens = re.findall(r"[\w]+", user_query, flags=re.UNICODE)
    if not tokens:
        raise ValueError("Search query did not contain any searchable terms")
    return " AND ".join(tokens)


def search(output_dir=DEFAULT_OUTPUT_DIR, query=None, limit=10):
    if not query:
        raise ValueError("Missing search query")
    index_path = Path(output_dir) / "index.sqlite"
    if not index_path.exists():
        raise RuntimeError(f"Missing index: {index_path}")
    connection = sqlite3.connect(index_path)
    connection.row_factory = sqlite3.Row
    try:
        sql = (
            "SELECT c.doc_id, c.chunk_id, c.kind, c.label, c.heading, c.page_start, c.page_end, "
            "snippet(chunks_fts, 1, '[', ']', '...', 24) AS snippet, bm25(chunks_fts) AS score "
            "FROM chunks_fts JOIN chunks c ON chunks_fts.rowid = c.rowid "
            "WHERE chunks_fts MATCH ? ORDER BY score LIMIT ?"
        )
        return [dict(row) for row in connection.execute(sql, (fts_query(query), limit))]
    finally:
        connection.close()


def read_chunk_from_jsonl(output_dir, chunk_id):
    chunks_dir = Path(output_dir) / "chunks"
    for path in sorted(chunks_dir.glob("*.jsonl")):
        with path.open(encoding="utf-8") as stream:
            for line in stream:
                chunk = json.loads(line)
                if chunk.get("chunk_id") == chunk_id:
                    return chunk
    return None


def show(output_dir=DEFAULT_OUTPUT_DIR, identifier=None):
    if not identifier:
        raise ValueError("Missing chunk or page identifier")
    page_match = re.match(r"^(.+):page:(\d+)$", identifier)
    if page_match:
        doc_id = page_match.group(1)
        page = int(page_match.group(2))
        page_path = Path(output_dir) / "pages" / doc_id / f"page-{page:04d}.txt"
        if not page_path.exists():
            raise RuntimeError(f"Missing page artifact: {page_path}")
        return {
            "type": "page",
            "doc_id": doc_id,
            "page_start": page,
            "page_end": page,
            "heading": f"{doc_id} page {page}",
            "text": page_path.read_text(encoding="utf-8"),
        }

    chunk = None
    index_path = Path(output_dir) / "index.sqlite"
    if index_path.exists():
        connection = sqlite3.connect(index_path)
        connection.row_factory = sqlite3.Row
        try:
            row = connection.execute("SELECT * FROM chunks WHERE chunk_id = ?", (identifier,)).fetchone()
            if row:
                chunk = dict(row)
        finally:
            connection.close()
    if chunk is None:
        chunk = read_chunk_from_jsonl(output_dir, identifier)
    if chunk is None:
        raise RuntimeError(f"Unknown chunk identifier: {identifier}")
    chunk["type"] = "chunk"
    return chunk


def status(output_dir=DEFAULT_OUTPUT_DIR, standards_dir=DEFAULT_STANDARDS_DIR, pdfs=None):
    pdf_paths = [Path(pdf) for pdf in pdfs] if pdfs else discover_pdfs(standards_dir)
    manifest = load_manifest(output_dir)
    current_documents = expected_pdf_entries(pdf_paths) if pdf_paths else []
    manifest_docs = {document["doc_id"]: document for document in manifest.get("documents", [])} if manifest else {}
    rows = []
    for document in current_documents:
        recorded = manifest_docs.get(document["doc_id"])
        if recorded is None:
            state = "missing"
        elif recorded.get("sha256") != document["sha256"]:
            state = "stale"
        elif manifest.get("pipeline_version") != PIPELINE_VERSION:
            state = "stale"
        elif manifest.get("extraction", {}).get("page_spec") is not None:
            state = "partial"
        elif not artifacts_exist(output_dir, recorded):
            state = "stale"
        else:
            state = "fresh"
        rows.append({**document, "state": state})
    return {
        "output_dir": str(output_dir),
        "pipeline_version": PIPELINE_VERSION,
        "tool_versions": {
            "pdftotext": tool_version("pdftotext"),
            "pdfinfo": tool_version("pdfinfo"),
            "sqlite": sqlite3.sqlite_version,
        },
        "manifest": manifest,
        "documents": rows,
    }
