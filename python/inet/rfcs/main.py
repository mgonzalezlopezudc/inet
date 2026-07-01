import argparse
import json
import sys
from pathlib import Path

try:
    from . import processor
except ImportError:
    import processor


def add_common_options(parser):
    parser.add_argument("--rfcs-dir", default=str(processor.DEFAULT_RFCS_DIR), help="Directory containing the RFC catalog and generated artifacts")
    parser.add_argument("--catalog", default=str(processor.DEFAULT_CATALOG_PATH), help="Protocol-to-RFC catalog")


def create_parser():
    parser = argparse.ArgumentParser(description="Preprocess online RFC Editor documents for implementation lookup.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build", help="Download RFC text, chunk it, and build a protocol-scoped SQLite FTS index")
    add_common_options(build_parser)
    build_parser.add_argument("--protocol", required=True, help="Protocol name from the catalog")
    build_parser.add_argument("--rfc", action="append", type=int, default=None, help="RFC number to process; may be repeated")
    build_parser.add_argument("--force", action="store_true", help="Rebuild even when artifacts are fresh")
    build_parser.add_argument("--offline", action="store_true", help="Rebuild from cached text artifacts and manifest metadata")
    build_parser.add_argument("--accept-errata", action="store_true", help="Record verified errata as accepted annotations in the manifest")

    status_parser = subparsers.add_parser("status", help="Report RFC corpus freshness")
    add_common_options(status_parser)
    status_parser.add_argument("--protocol", default=None, help="Protocol name to check; by default all catalog protocols are checked")
    status_parser.add_argument("--offline", action="store_true", help="Check only catalog, manifest, and local artifacts")
    status_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    search_parser = subparsers.add_parser("search", help="Search generated RFC SQLite FTS indexes")
    add_common_options(search_parser)
    scope = search_parser.add_mutually_exclusive_group(required=True)
    scope.add_argument("--protocol", help="Protocol name to search")
    scope.add_argument("--all", action="store_true", help="Search all protocol indexes from the catalog")
    search_parser.add_argument("query", help="Search query")
    search_parser.add_argument("-n", "--limit", type=int, default=10, help="Maximum number of results")
    search_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    show_parser = subparsers.add_parser("show", help="Show a chunk id")
    add_common_options(show_parser)
    show_parser.add_argument("identifier", help="Chunk id, e.g. rfc8200:chunk:00001")
    show_parser.add_argument("--protocol", default=None, help="Protocol name to restrict lookup")
    show_parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")
    return parser


def print_build_result(result):
    print(f"{result['status']}: {result['output_dir']}")
    for document in result["documents"]:
        artifacts = document.get("artifacts", {})
        print(
            f"{result['protocol']}: {document['doc_id']} chunks={artifacts.get('chunk_count', '?')} "
            f"sha256={document['sha256'][:12]} errata={document.get('errata_snapshot', {}).get('total', 0)}"
        )


def print_status(result):
    print(f"pipeline_version={result['pipeline_version']} rfcs_dir={result['rfcs_dir']}")
    for protocol in result["protocols"]:
        print(f"{protocol['state']:14} {protocol['protocol']} rfcs={','.join(str(rfc) for rfc in protocol['rfcs'])}")
        for document in protocol["documents"]:
            print(f"  {document['state']:14} {document['doc_id']:8} sha256={document.get('sha256', '')[:12]}")


def print_search_results(rows):
    for row in rows:
        pages = f"p{row['page_start']}" if row["page_start"] == row["page_end"] else f"pp{row['page_start']}-{row['page_end']}"
        label = row["label"] or row["kind"]
        protocol = f"{row['protocol']}  " if row.get("protocol") else ""
        section = f"  sec {row['section']}" if row.get("section") else ""
        print(f"{protocol}{row['chunk_id']}  {row['doc_id']}  {pages}  {label}{section}")
        if row["heading"]:
            print(f"  {row['heading']}")
        if row["snippet"]:
            print(f"  {row['snippet']}")


def print_show_result(item):
    pages = f"p{item['page_start']}" if item["page_start"] == item["page_end"] else f"pp{item['page_start']}-{item['page_end']}"
    section = f" section {item['section']}" if item.get("section") else ""
    print(f"{item['chunk_id']}  {pages}{section}  {item.get('heading', '')}")
    print()
    print(item["text"].rstrip())


def main(argv=None):
    args = create_parser().parse_args(argv)
    try:
        if args.command == "build":
            result = processor.build(
                rfcs_dir=Path(args.rfcs_dir),
                catalog_path=Path(args.catalog),
                protocol=args.protocol,
                rfcs=args.rfc,
                force=args.force,
                offline=args.offline,
                accept_errata=args.accept_errata,
            )
            print_build_result(result)
        elif args.command == "status":
            result = processor.status(
                rfcs_dir=Path(args.rfcs_dir),
                catalog_path=Path(args.catalog),
                protocol=args.protocol,
                offline=args.offline,
            )
            if args.json:
                print(json.dumps(result, indent=2, ensure_ascii=False))
            else:
                print_status(result)
        elif args.command == "search":
            rows = processor.search(
                rfcs_dir=Path(args.rfcs_dir),
                catalog_path=Path(args.catalog),
                protocol=args.protocol,
                all_protocols=args.all,
                query=args.query,
                limit=args.limit,
            )
            if args.json:
                print(json.dumps(rows, indent=2, ensure_ascii=False))
            else:
                print_search_results(rows)
        elif args.command == "show":
            item = processor.show(
                rfcs_dir=Path(args.rfcs_dir),
                catalog_path=Path(args.catalog),
                identifier=args.identifier,
                protocol=args.protocol,
            )
            if args.json:
                print(json.dumps(item, indent=2, ensure_ascii=False))
            else:
                print_show_result(item)
        return 0
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
