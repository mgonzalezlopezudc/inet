#!/usr/bin/env python3
"""Run the shared PCAP pipeline for a declarative IEEE 802.11 suite."""

import argparse
import sys
from pathlib import Path


ANALYSIS_ROOT = Path(__file__).resolve().parent
REPOSITORY_ROOT = ANALYSIS_ROOT.parents[2]
AX_COMPATIBILITY_ROOT = REPOSITORY_ROOT / "examples" / "ieee80211ax" / "analysis"
sys.path.insert(0, str(ANALYSIS_ROOT))
sys.path.insert(0, str(AX_COMPATIBILITY_ROOT))

from inet_wifi_analysis import load_suite
import analyze_pcap_types


def main() -> None:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument(
        "--suite",
        type=Path,
        required=True,
        help="suite descriptor under examples/ieee80211/analysis/suites",
    )
    selected, remaining = parser.parse_known_args()
    suite = load_suite(selected.suite, REPOSITORY_ROOT)
    output_dir = ANALYSIS_ROOT / "generated" / suite.suite
    analyze_pcap_types.configure_suite(suite, output_dir)
    sys.argv = [sys.argv[0], *remaining]
    analyze_pcap_types.main()


if __name__ == "__main__":
    main()
