#!/usr/bin/env python3
"""Generate independent E1-02 version-2 structural wire-vector captures."""

import hashlib
from pathlib import Path
import struct

from generate_ieee802154_vectors import fcs


def main():
    # Table 7-6: H/P/D combinations in its published order.  Each vector
    # begins with a suppressed-sequence version-2 DATA FCF.  IE descriptors,
    # terminators and opaque data are written directly, not by INET.
    rows = [
        "0121", "01230109aa", "0123003f0290bbcc",
        "01230109aa003f0290bbcc", "0121dead",
        "01230109aa803fdead", "0123003f0290bbcc00f8dead",
        "01230109aa003f0290bbcc00f8dead",
        "0123003f0290bbcc00f8",  # Optional Payload Termination.
        "092121a1a2a3a4",       # Security level 1, suppressed counter, 4-byte MIC.
        "092124010203",         # Reserved level 4: opaque forensic remainder.
        "0123003f06880122aa0198bb00f8dead",  # MLME: unknown short/long, then MSDU.
        "0123003f04880001009000f8beef",      # Empty short/long, then MSDU.
    ]
    output = Path(__file__).resolve().parents[1] / "pcap" / "generated"
    for has_fcs, link_type in [(True, 195), (False, 230)]:
        result = bytearray(struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, link_type))
        for index, encoded in enumerate(rows):
            frame = bytes.fromhex(encoded)
            wire = frame + struct.pack("<H", fcs(frame)) if has_fcs else frame
            result += struct.pack("<IIII", 1, index * 1000, len(wire), len(wire)) + wire
        path = output / ("ieee802154-e102-with-fcs.pcap" if has_fcs else "ieee802154-e102-no-fcs.pcap")
        path.write_bytes(result)
        print(path.name, hashlib.sha256(result).hexdigest())


if __name__ == "__main__":
    main()
