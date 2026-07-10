---
name: ieee80211-standards
description: Search and inspect IEEE 802.11 standards stored in this repository. Use for questions about clauses, tables, figures, fields, procedures, or normative behavior in 802.11 standards, especially 802.11ax and 802.11be.
---

# Using the IEEE 802.11 standards corpus

The source standards documents are stored under:

```text
standards/
```

Prefer the generated searchable corpus over directly reading or reprocessing PDF files.

## Check corpus status

Run from the repository root:

```sh
inet_process_standards status
```

Use the status output to determine whether the corpus exists and is current.

## Search the corpus

Search using distinctive standard text, a clause number, table number, figure number, field name, or procedure name:

```sh
inet_process_standards search "Table 27-61"
```

Use several focused searches when the initial phrase is ambiguous:

```sh
inet_process_standards search "Table 27-61"
inet_process_standards search "HE-SIG-A"
inet_process_standards search "DL MU-MIMO"
```

Do not assume the first result is authoritative when the phrase occurs in multiple revisions or amendments.

## Show a corpus chunk

Display a selected result using its corpus identifier:

```sh
inet_process_standards show 80211ax-2024:chunk:00001
```

Preserve the document and chunk identifier when reporting the finding.

## Rebuild the corpus

If the status command reports that the corpus is missing or stale:

```sh
inet_process_standards build
```

After rebuilding, run:

```sh
inet_process_standards status
```

and repeat the search.

## Generated files

Generated text, page files, chunks, and the SQLite full-text-search index are stored under:

```text
standards/processed/
```

These generated files are intentionally ignored by Git.

Do not:

* Add `standards/processed/` to version control.
* Treat generated text as a hand-maintained source file.
* Edit generated chunks or indexes manually.
* Commit corpus rebuild output unless repository policy explicitly changes.

## When to read the PDFs directly

Read or process a source PDF only when:

* The generated corpus cannot answer the question.
* Exact visual layout is required.
* A figure, diagram, equation, or table structure must be inspected.
* Page-level verification is required.
* The corpus appears to contain an extraction error.
* The user explicitly asks for the original document.

When consulting a PDF, identify the exact standard revision, clause or annex, and page.

Do not rely on a visually similar table from another revision without confirming that it belongs to the requested document.

## Investigation procedure

1. Run `inet_process_standards status`.
2. Build the corpus if missing or stale.
3. Search for the exact clause, table, figure, or term.
4. Open the most relevant chunks.
5. Check whether the result comes from the correct standard revision.
6. Search for definitions and cross-referenced clauses when necessary.
7. Consult the original PDF only for unresolved or visual details.
8. Distinguish normative requirements from notes, examples, and informative text.

Report the document revision, search terms and chunk identifiers, clause/table/figure, whether the PDF was needed, relevant cross-references, revision ambiguity, and normative or informative status when material.
