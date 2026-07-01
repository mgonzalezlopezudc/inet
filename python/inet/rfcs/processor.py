import hashlib
import html
import json
import os
import platform
import re
import sqlite3
import urllib.error
import urllib.request
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


PIPELINE_VERSION = "1"
DEFAULT_RFCS_DIR = Path("standards") / "RFCs"
DEFAULT_CATALOG_PATH = DEFAULT_RFCS_DIR / "catalog.json"
RFC_EDITOR_RFC_URL = "https://www.rfc-editor.org/rfc/rfc{number}.txt"
RFC_EDITOR_INFO_URL = "https://www.rfc-editor.org/info/rfc{number}/"
RFC_EDITOR_ERRATA_URL = "https://www.rfc-editor.org/errata_search.php?rfc={number}"
USER_AGENT = "INET RFC corpus processor/1"

TOC_LEADER_RE = re.compile(r"(?:\s+\.\s*){3,}\s*\d+\s*$")
SECTION_RE = re.compile(r"^\s*((?:\d+\.)+\d+|\d+)\.?\s+([A-Z][^.\n]{1,180})\s*$")
APPENDIX_RE = re.compile(r"^\s*Appendix\s+([A-Z](?:\.\d+)*)\.?\s+(.{1,180})\s*$")
REFERENCE_RE = re.compile(r"^\s*(?:(\d+(?:\.\d+)*)\.?\s+)?((?:Normative|Informative)\s+References|References)\s*$")
FIGURE_TABLE_RE = re.compile(r"^\s*(Figure|Table)\s+([A-Za-z]?\d+(?:[-.]\d+)*)[:. -]+(.{1,180})\s*$")
PAGE_FOOTER_RE = re.compile(r"^\s*.+\s+\[Page\s+(\d+)\]\s*$")
PAGE_HEADER_RE = re.compile(r"^\s*RFC\s+\d+\s+.+\s+(?:January|February|March|April|May|June|July|August|September|October|November|December)\s+\d{4}\s*$")
INFO_TITLE_RE = re.compile(r"<h1[^>]*>\s*RFC\s*\d+[^:>]*:\s*(.*?)\s*</h1>", re.IGNORECASE | re.DOTALL)


@dataclass
class Heading:
    kind: str
    label: str
    heading: str
    section: str


@dataclass
class Chunk:
    protocol: str
    doc_id: str
    chunk_id: str
    kind: str
    label: str
    heading: str
    section: str
    page_start: int
    page_end: int
    text: str

    def to_json(self):
        return {
            "protocol": self.protocol,
            "doc_id": self.doc_id,
            "chunk_id": self.chunk_id,
            "kind": self.kind,
            "label": self.label,
            "heading": self.heading,
            "section": self.section,
            "page_start": self.page_start,
            "page_end": self.page_end,
            "text": self.text,
        }


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def normalize_text(text):
    return text.replace("\r\n", "\n").replace("\r", "\n")


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_text(text):
    return sha256_bytes(text.encode("utf-8"))


def protocol_dir_name(protocol):
    if not protocol or protocol in {".", ".."} or os.path.isabs(protocol) or "/" in protocol or "\\" in protocol:
        raise ValueError(f"Invalid protocol name for RFC corpus path: {protocol!r}")
    return protocol


def protocol_output_dir(rfcs_dir, protocol):
    return Path(rfcs_dir) / protocol_dir_name(protocol) / "processed"


def read_catalog(catalog_path=DEFAULT_CATALOG_PATH):
    path = Path(catalog_path)
    if not path.exists():
        return {"protocols": {}}
    catalog = json.loads(path.read_text(encoding="utf-8"))
    protocols = catalog.get("protocols", {})
    normalized = {}
    for protocol, value in protocols.items():
        rfcs = value.get("rfcs", value) if isinstance(value, dict) else value
        normalized[protocol] = sorted(dict.fromkeys(int(rfc) for rfc in rfcs))
    return {"protocols": normalized}


def resolve_rfcs(catalog, protocol, rfcs=None):
    if rfcs:
        return sorted(dict.fromkeys(int(rfc) for rfc in rfcs))
    protocols = catalog.get("protocols", {})
    if protocol not in protocols:
        raise RuntimeError(f"Unknown RFC protocol {protocol!r}; add it to the catalog or pass --rfc")
    return protocols[protocol]


def fetch_url(url, timeout=30):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        body = response.read()
        headers = {key.lower(): value for key, value in response.headers.items()}
    return {
        "url": url,
        "body": body,
        "headers": headers,
        "sha256": sha256_bytes(body),
        "byte_length": len(body),
        "fetched_at": utc_now(),
    }


def parse_csv_numbers(value):
    if not value:
        return []
    numbers = []
    for number in re.findall(r"\b\d{3,5}\b", value):
        numbers.append(int(number))
    return sorted(dict.fromkeys(numbers))


def parse_header_relationships(text):
    header = "\n".join(normalize_text(text).splitlines()[:80])
    relationships = {"updates": [], "obsoletes": []}
    for key, target in [("updates", "Updates"), ("obsoletes", "Obsoletes")]:
        match = re.search(rf"\b{target}:\s*([0-9,\s]+)", header, re.IGNORECASE)
        if match:
            relationships[key] = parse_csv_numbers(match.group(1))
    return relationships


def parse_title_from_text(number, text):
    lines = [line.rstrip() for line in normalize_text(text).splitlines()]
    marker_index = None
    for index, line in enumerate(lines):
        if line.strip() in {"Status of This Memo", "Abstract"}:
            marker_index = index
            break
    if marker_index is not None:
        title_block = []
        block_index = marker_index - 1
        while block_index >= 0 and not lines[block_index].strip():
            block_index -= 1
        while block_index >= 0 and lines[block_index].strip():
            title_block.append(lines[block_index].strip())
            block_index -= 1
        title_block = list(reversed(title_block))
        title_block = [
            candidate for candidate in title_block
            if not re.match(r"^(?:January|February|March|April|May|June|July|August|September|October|November|December)\s+\d{4}$", candidate)
        ]
        if title_block:
            return " ".join(title_block)

        candidates = [candidate.strip() for candidate in lines[max(0, marker_index - 14):marker_index] if candidate.strip()]
        candidates = [
            candidate for candidate in candidates
            if not candidate.startswith(("ISSN:", "Category:", "Request for Comments:", "Updates:", "Obsoletes:", "STD:"))
            and candidate not in {"Copyright Notice"}
            and not re.match(r"^(?:January|February|March|April|May|June|July|August|September|October|November|December)\s+\d{4}$", candidate)
            and "Network Working Group" not in candidate
            and "Internet Engineering Task Force" not in candidate
        ]
        if candidates:
            title_lines = candidates[-2:] if len(candidates) >= 2 and len(candidates[-2]) < 78 and len(candidates[-1]) < 78 else candidates[-1:]
            return " ".join(title_lines)
    for line in lines[:40]:
        stripped = line.strip()
        if stripped and f"RFC {number}" not in stripped and "Request for Comments" not in stripped:
            if not stripped.startswith(("Internet Engineering Task Force", "Network Working Group", "Category:", "ISSN:", "Updates:", "Obsoletes:")):
                return stripped
    return f"RFC {number}"


def html_lines(body):
    text = body.decode("utf-8", errors="replace")
    text = re.sub(r"<[^>]+>", "\n", text)
    text = html.unescape(text)
    return [line.strip() for line in text.splitlines() if line.strip()]


def rfc_numbers_near(lines, index, window=6):
    selected = []
    found_rfc = False
    stop_labels = {"references", "referenced by", "authors", "formats", "rfc stream", "additional resources", "iesg"}
    for offset, line in enumerate(lines[index:index + window]):
        lowered = line.lower()
        if offset > 0 and (lowered in stop_labels or lowered.startswith("last updated") or lowered.startswith("was ")):
            break
        if offset > 0 and found_rfc and not re.search(r"\bRFC\s*\d{3,5}\b", line, re.IGNORECASE):
            break
        selected.append(line)
        if re.search(r"\bRFC\s*\d{3,5}\b", line, re.IGNORECASE):
            found_rfc = True
    segment = " ".join(selected)
    return sorted(dict.fromkeys(int(match) for match in re.findall(r"\bRFC\s*(\d{3,5})\b", segment, re.IGNORECASE)))


def clean_info_title(title):
    title = re.sub(r"\s*\|\s*RFC Editor\s*$", "", title).strip()
    return title


def parse_info_metadata(number, body):
    text = body.decode("utf-8", errors="replace")
    lines = html_lines(body)
    metadata = {
        "info_url": RFC_EDITOR_INFO_URL.format(number=number),
        "updated_by": [],
        "obsoleted_by": [],
        "status": "",
        "published": "",
    }
    title_match = INFO_TITLE_RE.search(text)
    if title_match:
        metadata["title"] = clean_info_title(html.unescape(re.sub(r"<[^>]+>", "", title_match.group(1))).strip())
    if not metadata.get("title"):
        for line in lines:
            match = re.match(rf"^RFC\s*{number}\b[^:]*:\s*(.+)$", line, re.IGNORECASE)
            if match:
                metadata["title"] = clean_info_title(match.group(1).strip())
                break
    for index, line in enumerate(lines):
        lowered = line.lower()
        if "updated by" in lowered or "this rfc was updated" in lowered:
            metadata["updated_by"] = [rfc for rfc in rfc_numbers_near(lines, index) if rfc != int(number)]
        elif "obsoleted by" in lowered or "this rfc was obsoleted" in lowered:
            metadata["obsoleted_by"] = [rfc for rfc in rfc_numbers_near(lines, index) if rfc != int(number)]
        elif line.startswith("Document Type"):
            metadata["status"] = line.removeprefix("Document Type").strip()
    updated_match = re.search(r"Last updated\s+([0-9-]+)", text, re.IGNORECASE)
    if updated_match:
        metadata["last_updated"] = updated_match.group(1)
    return metadata


def fetch_errata_snapshot(number):
    url = RFC_EDITOR_ERRATA_URL.format(number=number)
    try:
        resource = fetch_url(url)
    except urllib.error.URLError as error:
        return {"available": False, "source_url": url, "error": str(error), "total": 0, "ids_by_status": {}, "counts_by_status": {}}
    body = resource["body"].decode("utf-8", errors="replace")
    ids_by_status = {}
    for match in re.finditer(r"(?:Errata ID|EID)\s*:?\s*(\d+).*?(Reported|Verified|Rejected|Held for Document Update)", body, re.IGNORECASE | re.DOTALL):
        status = " ".join(match.group(2).title().split())
        ids_by_status.setdefault(status, []).append(int(match.group(1)))
    for status, ids in list(ids_by_status.items()):
        ids_by_status[status] = sorted(dict.fromkeys(ids))
    counts_by_status = {status: len(ids) for status, ids in ids_by_status.items()}
    return {
        "available": True,
        "source_url": url,
        "source_sha256": resource["sha256"],
        "fetched_at": resource["fetched_at"],
        "etag": resource["headers"].get("etag"),
        "last_modified": resource["headers"].get("last-modified"),
        "total": sum(counts_by_status.values()),
        "ids_by_status": ids_by_status,
        "counts_by_status": counts_by_status,
    }


def fetch_rfc_document(number, accept_errata=False):
    txt = fetch_url(RFC_EDITOR_RFC_URL.format(number=number))
    text = normalize_text(txt["body"].decode("utf-8", errors="replace"))
    metadata = {
        "doc_id": f"rfc{number}",
        "rfc_number": int(number),
        "source_url": txt["url"],
        "sha256": txt["sha256"],
        "byte_length": txt["byte_length"],
        "fetched_at": txt["fetched_at"],
        "etag": txt["headers"].get("etag"),
        "last_modified": txt["headers"].get("last-modified"),
        "title": parse_title_from_text(number, text),
        "published": "",
        "status": "",
        "updates": [],
        "obsoletes": [],
        "updated_by": [],
        "obsoleted_by": [],
    }
    metadata.update(parse_header_relationships(text))
    try:
        info = fetch_url(RFC_EDITOR_INFO_URL.format(number=number))
        metadata.update({key: value for key, value in parse_info_metadata(number, info["body"]).items() if value not in ("", [], None)})
    except urllib.error.URLError:
        pass
    metadata["errata_snapshot"] = fetch_errata_snapshot(number)
    metadata["accepted_errata"] = accept_errata
    return {"text": text, "metadata": metadata}


def split_rfc_pages(text):
    text = normalize_text(text)
    if "\f" in text:
        raw_pages = text.split("\f")
        if raw_pages and raw_pages[-1] == "":
            raw_pages = raw_pages[:-1]
        return [(index + 1, page) for index, page in enumerate(raw_pages)]

    pages = []
    current_lines = []
    current_page = 1
    for line in text.splitlines():
        footer = PAGE_FOOTER_RE.match(line)
        if footer:
            page = int(footer.group(1))
            pages.append((page, "\n".join(current_lines)))
            current_lines = []
            current_page = page + 1
            continue
        current_lines.append(line)
    if current_lines or not pages:
        pages.append((current_page, "\n".join(current_lines)))
    return pages


def clean_page_text(text):
    lines = []
    for line in normalize_text(text).splitlines():
        if PAGE_HEADER_RE.match(line):
            continue
        if PAGE_FOOTER_RE.match(line):
            continue
        lines.append(line.rstrip())
    return "\n".join(lines).strip()


def is_probable_toc_line(line):
    stripped = line.strip()
    return bool(TOC_LEADER_RE.search(stripped)) or stripped.count(".") > 12


def detect_heading(line):
    stripped = line.strip()
    if len(stripped) < 4 or is_probable_toc_line(stripped):
        return None

    match = FIGURE_TABLE_RE.match(stripped)
    if match:
        kind = match.group(1).lower()
        label = f"{match.group(1)} {match.group(2)}"
        heading = f"{label} {match.group(3).strip()}".strip()
        return Heading(kind=kind, label=label, heading=heading, section="")

    match = APPENDIX_RE.match(stripped)
    if match:
        section = f"Appendix {match.group(1)}"
        heading = f"{section} {match.group(2).strip()}"
        return Heading(kind="appendix", label=section, heading=heading, section=section)

    match = REFERENCE_RE.match(stripped)
    if match:
        section = match.group(1) or ""
        label = match.group(2)
        heading = f"{section} {label}".strip()
        return Heading(kind="references", label=label, heading=heading, section=section)

    match = SECTION_RE.match(stripped)
    if match:
        section = match.group(1).rstrip(".")
        title = match.group(2).strip()
        return Heading(kind="section", label=section, heading=f"{section} {title}", section=section)

    return None


def chunk_rfc_text(protocol, doc_id, text):
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
        body = "\n".join(current_lines).strip()
        if body:
            chunk_id = f"{doc_id}:chunk:{len(chunks) + 1:05d}"
            chunks.append(Chunk(
                protocol=protocol,
                doc_id=doc_id,
                chunk_id=chunk_id,
                kind=current.kind,
                label=current.label,
                heading=current.heading,
                section=current.section,
                page_start=current_start,
                page_end=current_end,
                text=body,
            ))
        current = None
        current_lines = []
        current_start = None
        current_end = None

    for page_number, page_text in split_rfc_pages(text):
        cleaned = clean_page_text(page_text)
        if not cleaned:
            continue
        if current is None:
            current = Heading(kind="page", label=f"Page {page_number}", heading=f"Page {page_number}", section="")
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
    for subdir in ["text", "chunks"]:
        (output_dir / subdir).mkdir(parents=True, exist_ok=True)
    return output_dir


def write_doc_artifacts(output_dir, protocol, document, text):
    output_dir = Path(output_dir)
    doc_id = document["doc_id"]
    text_path = output_dir / "text" / f"{doc_id}.txt"
    text_path.write_text(text, encoding="utf-8")
    chunks = chunk_rfc_text(protocol, doc_id, text)
    chunk_path = output_dir / "chunks" / f"{doc_id}.jsonl"
    with chunk_path.open("w", encoding="utf-8") as stream:
        for chunk in chunks:
            stream.write(json.dumps(chunk.to_json(), ensure_ascii=False) + "\n")
    return {
        "text": str(text_path),
        "chunks": str(chunk_path),
        "chunk_count": len(chunks),
    }, chunks


def relationship_json(document, key):
    return json.dumps(document.get(key, []), sort_keys=True)


def build_sqlite_index(output_dir, protocol, chunks_by_doc, documents):
    ensure_fts5_available()
    index_path = Path(output_dir) / "index.sqlite"
    if index_path.exists():
        index_path.unlink()
    connection = sqlite3.connect(index_path)
    try:
        connection.execute(
            "CREATE TABLE documents ("
            "doc_id TEXT PRIMARY KEY, protocol TEXT, rfc_number INTEGER, source_url TEXT, sha256 TEXT, "
            "title TEXT, published TEXT, status TEXT, updates_json TEXT, obsoletes_json TEXT, "
            "updated_by_json TEXT, obsoleted_by_json TEXT, errata_json TEXT)"
        )
        connection.execute(
            "CREATE TABLE chunks ("
            "protocol TEXT NOT NULL, doc_id TEXT NOT NULL, chunk_id TEXT PRIMARY KEY, kind TEXT NOT NULL, "
            "label TEXT, heading TEXT, section TEXT, page_start INTEGER NOT NULL, page_end INTEGER NOT NULL, text TEXT NOT NULL)"
        )
        connection.execute("CREATE VIRTUAL TABLE chunks_fts USING fts5(heading, text, content='chunks', content_rowid='rowid')")
        for document in documents:
            connection.execute(
                "INSERT INTO documents(doc_id, protocol, rfc_number, source_url, sha256, title, published, status, "
                "updates_json, obsoletes_json, updated_by_json, obsoleted_by_json, errata_json) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                (
                    document["doc_id"],
                    protocol,
                    document.get("rfc_number"),
                    document.get("source_url", ""),
                    document.get("sha256", ""),
                    document.get("title", ""),
                    document.get("published", ""),
                    document.get("status", ""),
                    relationship_json(document, "updates"),
                    relationship_json(document, "obsoletes"),
                    relationship_json(document, "updated_by"),
                    relationship_json(document, "obsoleted_by"),
                    json.dumps(document.get("errata_snapshot", {}), sort_keys=True),
                ),
            )
        for chunks in chunks_by_doc.values():
            for chunk in chunks:
                cursor = connection.execute(
                    "INSERT INTO chunks(protocol, doc_id, chunk_id, kind, label, heading, section, page_start, page_end, text) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (chunk.protocol, chunk.doc_id, chunk.chunk_id, chunk.kind, chunk.label, chunk.heading, chunk.section, chunk.page_start, chunk.page_end, chunk.text),
                )
                connection.execute("INSERT INTO chunks_fts(rowid, heading, text) VALUES (?, ?, ?)", (cursor.lastrowid, chunk.heading, chunk.text))
        connection.commit()
    finally:
        connection.close()
    return index_path


def write_manifest(output_dir, protocol, rfcs, documents, accept_errata):
    manifest = {
        "pipeline_version": PIPELINE_VERSION,
        "generated_at": utc_now(),
        "python_version": platform.python_version(),
        "protocol": protocol,
        "rfcs": list(rfcs),
        "source": {
            "rfc_editor_rfc_url": RFC_EDITOR_RFC_URL,
            "rfc_editor_info_url": RFC_EDITOR_INFO_URL,
            "rfc_editor_errata_url": RFC_EDITOR_ERRATA_URL,
        },
        "freshness": {
            "accept_errata": accept_errata,
            "errata_are_body_external": True,
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


def artifacts_exist(output_dir, document):
    doc_id = document["doc_id"]
    return (
        (Path(output_dir) / "text" / f"{doc_id}.txt").exists()
        and (Path(output_dir) / "chunks" / f"{doc_id}.jsonl").exists()
        and (Path(output_dir) / "index.sqlite").exists()
    )


def comparable_metadata(document):
    keys = ["title", "published", "status", "updates", "obsoletes", "updated_by", "obsoleted_by"]
    return {key: document.get(key, [] if key.endswith(("by", "tes")) else "") for key in keys}


def protocol_state_from_documents(document_states):
    if any(state in {"missing", "stale"} for state in document_states):
        return "stale"
    if any(state == "metadata-stale" for state in document_states):
        return "metadata-stale"
    if any(state == "errata-stale" for state in document_states):
        return "errata-stale"
    return "fresh"


def evaluate_protocol_state(output_dir, manifest, protocol, rfcs, current_documents=None):
    if not manifest:
        return "stale", [{"doc_id": f"rfc{rfc}", "rfc_number": rfc, "state": "missing"} for rfc in rfcs]
    if manifest.get("pipeline_version") != PIPELINE_VERSION or manifest.get("protocol") != protocol or manifest.get("rfcs") != list(rfcs):
        return "stale", [{"doc_id": f"rfc{rfc}", "rfc_number": rfc, "state": "stale"} for rfc in rfcs]

    recorded_docs = {document["doc_id"]: document for document in manifest.get("documents", [])}
    if current_documents is None:
        rows = []
        for rfc in rfcs:
            doc_id = f"rfc{rfc}"
            recorded = recorded_docs.get(doc_id)
            state = "missing" if recorded is None else "fresh"
            if recorded is not None and not artifacts_exist(output_dir, recorded):
                state = "stale"
            rows.append({**(recorded or {"doc_id": doc_id, "rfc_number": rfc}), "state": state})
        return protocol_state_from_documents([row["state"] for row in rows]), rows

    rows = []
    for current in current_documents:
        recorded = recorded_docs.get(current["doc_id"])
        if recorded is None:
            state = "missing"
        elif recorded.get("sha256") != current.get("sha256"):
            state = "stale"
        elif not artifacts_exist(output_dir, recorded):
            state = "stale"
        elif comparable_metadata(recorded) != comparable_metadata(current):
            state = "metadata-stale"
        elif recorded.get("errata_snapshot") != current.get("errata_snapshot"):
            state = "errata-stale"
        else:
            state = "fresh"
        rows.append({**current, "state": state})
    return protocol_state_from_documents([row["state"] for row in rows]), rows


def read_cached_document(output_dir, rfc_number, manifest):
    doc_id = f"rfc{rfc_number}"
    text_path = Path(output_dir) / "text" / f"{doc_id}.txt"
    if not text_path.exists():
        raise RuntimeError(f"Missing cached RFC text: {text_path}")
    text = text_path.read_text(encoding="utf-8")
    recorded = {}
    if manifest:
        recorded = {document["doc_id"]: document for document in manifest.get("documents", [])}.get(doc_id, {})
    metadata = dict(recorded)
    metadata.update({
        "doc_id": doc_id,
        "rfc_number": int(rfc_number),
        "source_url": metadata.get("source_url", RFC_EDITOR_RFC_URL.format(number=rfc_number)),
        "sha256": sha256_text(text),
        "byte_length": len(text.encode("utf-8")),
        "title": metadata.get("title", parse_title_from_text(rfc_number, text)),
        "updates": metadata.get("updates", []),
        "obsoletes": metadata.get("obsoletes", []),
        "updated_by": metadata.get("updated_by", []),
        "obsoleted_by": metadata.get("obsoleted_by", []),
        "errata_snapshot": metadata.get("errata_snapshot", {"available": False, "total": 0, "ids_by_status": {}, "counts_by_status": {}}),
    })
    return {"text": text, "metadata": metadata}


def build(rfcs_dir=DEFAULT_RFCS_DIR, catalog_path=DEFAULT_CATALOG_PATH, protocol=None, rfcs=None, force=False, offline=False, accept_errata=False):
    if not protocol:
        raise ValueError("Missing protocol")
    catalog = read_catalog(catalog_path)
    rfc_numbers = resolve_rfcs(catalog, protocol, rfcs)
    output_dir = protocol_output_dir(rfcs_dir, protocol)
    manifest = load_manifest(output_dir)

    fetched = []
    if offline:
        for number in rfc_numbers:
            fetched.append(read_cached_document(output_dir, number, manifest))
    else:
        for number in rfc_numbers:
            fetched.append(fetch_rfc_document(number, accept_errata=accept_errata))

    expected_documents = [item["metadata"] for item in fetched]
    if not force:
        state, _ = evaluate_protocol_state(output_dir, manifest, protocol, rfc_numbers, expected_documents)
        if state == "fresh":
            return {"status": "fresh", "protocol": protocol, "documents": manifest["documents"], "output_dir": str(output_dir)}

    output_dir = prepare_output_dirs(output_dir)
    documents = []
    chunks_by_doc = {}
    for item in fetched:
        document = dict(item["metadata"])
        artifacts, chunks = write_doc_artifacts(output_dir, protocol, document, item["text"])
        document["artifacts"] = artifacts
        documents.append(document)
        chunks_by_doc[document["doc_id"]] = chunks

    build_sqlite_index(output_dir, protocol, chunks_by_doc, documents)
    write_manifest(output_dir, protocol, rfc_numbers, documents, accept_errata)
    return {"status": "built", "protocol": protocol, "documents": documents, "output_dir": str(output_dir)}


def fts_query(user_query):
    tokens = re.findall(r"[\w]+", user_query, flags=re.UNICODE)
    if not tokens:
        raise ValueError("Search query did not contain any searchable terms")
    return " AND ".join(tokens)


def search_protocol(output_dir, query, limit):
    index_path = Path(output_dir) / "index.sqlite"
    if not index_path.exists():
        return []
    connection = sqlite3.connect(index_path)
    connection.row_factory = sqlite3.Row
    try:
        sql = (
            "SELECT c.protocol, c.doc_id, c.chunk_id, c.kind, c.label, c.heading, c.section, c.page_start, c.page_end, "
            "snippet(chunks_fts, 1, '[', ']', '...', 24) AS snippet, bm25(chunks_fts) AS score "
            "FROM chunks_fts JOIN chunks c ON chunks_fts.rowid = c.rowid "
            "WHERE chunks_fts MATCH ? ORDER BY score LIMIT ?"
        )
        return [dict(row) for row in connection.execute(sql, (fts_query(query), limit))]
    finally:
        connection.close()


def search(rfcs_dir=DEFAULT_RFCS_DIR, catalog_path=DEFAULT_CATALOG_PATH, protocol=None, all_protocols=False, query=None, limit=10):
    if not query:
        raise ValueError("Missing search query")
    if all_protocols:
        catalog = read_catalog(catalog_path)
        rows = []
        for candidate in catalog.get("protocols", {}):
            rows.extend(search_protocol(protocol_output_dir(rfcs_dir, candidate), query, limit))
        return sorted(rows, key=lambda row: row.get("score", 0))[:limit]
    if not protocol:
        raise ValueError("Missing protocol")
    return search_protocol(protocol_output_dir(rfcs_dir, protocol), query, limit)


def read_chunk_from_jsonl(output_dir, chunk_id):
    chunks_dir = Path(output_dir) / "chunks"
    for path in sorted(chunks_dir.glob("*.jsonl")):
        with path.open(encoding="utf-8") as stream:
            for line in stream:
                chunk = json.loads(line)
                if chunk.get("chunk_id") == chunk_id:
                    return chunk
    return None


def show(rfcs_dir=DEFAULT_RFCS_DIR, catalog_path=DEFAULT_CATALOG_PATH, identifier=None, protocol=None):
    if not identifier:
        raise ValueError("Missing chunk identifier")
    protocols = [protocol] if protocol else list(read_catalog(catalog_path).get("protocols", {}).keys())
    for candidate in protocols:
        output_dir = protocol_output_dir(rfcs_dir, candidate)
        index_path = output_dir / "index.sqlite"
        chunk = None
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
        if chunk is not None:
            chunk["type"] = "chunk"
            return chunk
    raise RuntimeError(f"Unknown chunk identifier: {identifier}")


def status(rfcs_dir=DEFAULT_RFCS_DIR, catalog_path=DEFAULT_CATALOG_PATH, protocol=None, offline=False):
    catalog = read_catalog(catalog_path)
    protocols = [protocol] if protocol else list(catalog.get("protocols", {}).keys())
    rows = []
    for candidate in protocols:
        rfc_numbers = resolve_rfcs(catalog, candidate)
        output_dir = protocol_output_dir(rfcs_dir, candidate)
        manifest = load_manifest(output_dir)
        current_documents = None
        if not offline:
            current_documents = [fetch_rfc_document(number)["metadata"] for number in rfc_numbers]
        state, documents = evaluate_protocol_state(output_dir, manifest, candidate, rfc_numbers, current_documents)
        rows.append({
            "protocol": candidate,
            "rfcs": rfc_numbers,
            "output_dir": str(output_dir),
            "state": state,
            "documents": documents,
        })
    return {
        "rfcs_dir": str(rfcs_dir),
        "catalog": str(catalog_path),
        "pipeline_version": PIPELINE_VERSION,
        "protocols": rows,
    }
