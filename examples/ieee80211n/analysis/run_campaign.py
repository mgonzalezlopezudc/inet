#!/usr/bin/env python3
"""Run campaign delegate script for IEEE 802.11n scenarios."""

from __future__ import annotations

import sys
from pathlib import Path

ANALYSIS_ROOT = Path(__file__).resolve().parent
REPOSITORY_ROOT = ANALYSIS_ROOT.parents[2]
AX_ANALYSIS_ROOT = REPOSITORY_ROOT / "examples" / "ieee80211ax" / "analysis"

if str(AX_ANALYSIS_ROOT) not in sys.path:
    sys.path.insert(0, str(AX_ANALYSIS_ROOT))

from run_campaign import main

if __name__ == "__main__":
    main()
