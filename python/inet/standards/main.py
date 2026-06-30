import argparse
import json
import sys
from pathlib import Path

try:
    from . import processor
except ImportError:
    import processor


def add_common_options(parser):
    parser.add_argument("--standards-dir", default=str(processor.DEFAULT_STANDARDS_DIR), help="Directory containing source PDFs")
    parser.add_argument("--output", default=str(processor.DEFAULT_OUTPUT_DIR), help="Directory for generated artifacts")


def create_parser():
    parser = argparse.ArgumentParser(description="Preprocess local IEEE 802.11 standards PDFs for implementation lookup.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build", help="Extract text, chunks, and SQLite FTS index")
    add_common_options(build_parser)
    build_parser.add_argument("--pdf", action="append", default=None, help="PDF to process; may be repeated")
    build_parser.add_argument("--pages", default=None, help="Optional page range, e.g. 1-5 or 1-3,10")
    build_parser.add_argument("--force", action="store_true", help="Rebuild even when artifacts are fresh")

    search_parser = subparsers.add_parser("search", help="Search the generated SQLite FTS index")
    add_common_options(search_parser)
    search_parser.add_argument("query", help="Search query")
    search_parser.add_argument("-n", "--limit", type=int, default=10, help="Maximum number of results")
    search_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    show_parser = subparsers.add_parser("show", help="Show a chunk id or doc-id:page:N")
    add_common_options(show_parser)
    show_parser.add_argument("identifier", help="Chunk id or page id, e.g. 80211be-2024:chunk:00001")
    show_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    status_parser = subparsers.add_parser("status", help="Report artifact freshness")
    add_common_options(status_parser)
    status_parser.add_argument("--pdf", action="append", default=None, help="PDF to check; may be repeated")
    status_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")
    return parser


def print_build_result(result):
    print(f"{result['status']}: {result['output_dir']}")
    for document in result["documents"]:
        artifacts = document.get("artifacts", {})
        print(
            f"{document['doc_id']}: pages={document.get('pages_generated', document.get('pages_total', 0))} "
            f"chunks={artifacts.get('chunk_count', '?')} sha256={document['sha256'][:12]}"
        )


def print_search_results(rows):
    for row in rows:
        pages = f"p{row['page_start']}" if row["page_start"] == row["page_end"] else f"pp{row['page_start']}-{row['page_end']}"
        label = row["label"] or row["kind"]
        print(f"{row['chunk_id']}  {row['doc_id']}  {pages}  {label}")
        if row["heading"]:
            print(f"  {row['heading']}")
        if row["snippet"]:
            print(f"  {row['snippet']}")


def print_show_result(item):
    pages = f"p{item['page_start']}" if item["page_start"] == item["page_end"] else f"pp{item['page_start']}-{item['page_end']}"
    print(f"{item.get('chunk_id', item['doc_id'] + ':page:' + str(item['page_start']))}  {pages}  {item.get('heading', '')}")
    print()
    print(item["text"].rstrip())


def print_status(result):
    print(f"pipeline_version={result['pipeline_version']} output={result['output_dir']}")
    for name, version in result["tool_versions"].items():
        print(f"{name}: {version}")
    for document in result["documents"]:
        print(f"{document['state']:7} {document['doc_id']:20} pages={document['pages_total']} sha256={document['sha256'][:12]}")


def main(argv=None):
    args = create_parser().parse_args(argv)
    try:
        if args.command == "build":
            result = processor.build(
                output_dir=Path(args.output),
                standards_dir=Path(args.standards_dir),
                pdfs=args.pdf,
                page_spec=args.pages,
                force=args.force,
            )
            print_build_result(result)
        elif args.command == "search":
            rows = processor.search(output_dir=Path(args.output), query=args.query, limit=args.limit)
            if args.json:
                print(json.dumps(rows, indent=2, ensure_ascii=False))
            else:
                print_search_results(rows)
        elif args.command == "show":
            item = processor.show(output_dir=Path(args.output), identifier=args.identifier)
            if args.json:
                print(json.dumps(item, indent=2, ensure_ascii=False))
            else:
                print_show_result(item)
        elif args.command == "status":
            result = processor.status(output_dir=Path(args.output), standards_dir=Path(args.standards_dir), pdfs=args.pdf)
            if args.json:
                print(json.dumps(result, indent=2, ensure_ascii=False))
            else:
                print_status(result)
        return 0
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
