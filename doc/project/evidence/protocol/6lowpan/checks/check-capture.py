#!/usr/bin/env python3
"""Validate native IEEE 802.15.4 ICMPv6/UDP captures with independent TShark decoding."""
import argparse
import hashlib
import pathlib
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("captures", nargs="+", type=pathlib.Path)
args = parser.parse_args()
print("capture\tsha256\tframes\tmax_psdu_bytes\tvalid_fcs\tvalid_icmpv6\tvalid_udp")
for path in args.captures:
    result = subprocess.run([
        "tshark", "-o", "udp.check_checksum:TRUE", "-r", str(path), "-T", "fields",
        "-e", "frame.len", "-e", "wpan.fcs_ok", "-e", "icmpv6.checksum.status",
        "-e", "udp.checksum.status",
    ], check=True, text=True, capture_output=True)
    rows = [line.split("\t") for line in result.stdout.splitlines()]
    assert rows, f"empty capture: {path}"
    assert all(5 <= int(row[0]) <= 127 for row in rows), path
    assert all(row[1] == "True" for row in rows), f"invalid or absent FCS: {path}"
    icmp = [value for row in rows for value in row[2].split(",") if value]
    udp = [value for row in rows for value in row[3].split(",") if value]
    checksums = icmp + udp
    assert checksums and all(value == "1" for value in checksums), f"transport checksum: {path} {checksums}"
    print(f"{path}\t{hashlib.sha256(path.read_bytes()).hexdigest()}\t{len(rows)}\t"
          f"{max(int(row[0]) for row in rows)}\t{len(rows)}\t{len(icmp)}\t{len(udp)}")
