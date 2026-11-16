#!/usr/bin/env python3
"""Independently inspect native slotted retry captures with Scapy."""

import argparse
import hashlib
from pathlib import Path

import scapy
from scapy.layers.dot15d4 import Dot15d4Data, Dot15d4FCS
from scapy.utils import RawPcapReader


def check_capture(path, attempts, expect_ack):
    reader = RawPcapReader(str(path))
    data_dsns = []
    ack_dsns = []
    try:
        assert reader.linktype == 195, reader.linktype
        for raw, metadata in reader:
            assert len(raw) == metadata.caplen == metadata.wirelen
            frame = Dot15d4FCS(raw, stop_dissection_after=Dot15d4Data)
            assert frame.compute_fcs(raw[:-2]) == raw[-2:]
            if frame.fcf_frametype == 1:
                assert len(raw) == 19 and frame.fcf_framever == 1 and frame.fcf_ackreq == 1
                assert frame.fcf_panidcompress == 1
                assert frame.fcf_destaddrmode == frame.fcf_srcaddrmode == 2
                data = frame[Dot15d4Data]
                assert data.dest_panid == 0x1234 and data.dest_addr == 1 and data.src_addr == 2
                assert raw[-10:-2] == bytes([0x5A]) * 8
                data_dsns.append(frame.seqnum)
            elif frame.fcf_frametype == 2:
                assert len(raw) == 5 and frame.fcf_framever == 0
                assert frame.fcf_destaddrmode == frame.fcf_srcaddrmode == 0
                ack_dsns.append(frame.seqnum)
    finally:
        reader.close()
    assert len(data_dsns) == attempts and len(set(data_dsns)) == 1, data_dsns
    assert bool(ack_dsns) == expect_ack, ack_dsns
    assert all(dsn == data_dsns[0] for dsn in ack_dsns), ack_dsns
    print(f"PASS {path} DATA={len(data_dsns)} ACK={len(ack_dsns)} DSN={data_dsns[0]} sha256={hashlib.sha256(path.read_bytes()).hexdigest()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--attempts", type=int, required=True)
    parser.add_argument("--expect-ack", action="store_true")
    args = parser.parse_args()
    print("Independent decoder: Scapy", scapy.__version__)
    check_capture(args.capture, args.attempts, args.expect_ack)


if __name__ == "__main__":
    main()
