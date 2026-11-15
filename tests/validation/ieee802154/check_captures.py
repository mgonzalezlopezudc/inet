#!/usr/bin/env python3
"""Independent Scapy checks for source-derived ACK and native DATA captures."""

import argparse
import hashlib
from pathlib import Path

import scapy
from scapy.layers.dot15d4 import Dot15d4, Dot15d4Data, Dot15d4FCS
from scapy.utils import RawPcapReader


def records(path):
    reader = RawPcapReader(str(path))
    try:
        assert reader.linktype in (195, 230), reader.linktype
        for raw, metadata in reader:
            assert len(raw) == metadata.caplen == metadata.wirelen
            yield reader.linktype, raw
    finally:
        reader.close()


def check_ack(path, has_fcs):
    expected = bytes.fromhex("02006ae479" if has_fcs else "02006a")
    count = 0
    for linktype, raw in records(path):
        assert linktype == (195 if has_fcs else 230)
        assert raw == expected
        packet = (Dot15d4FCS if has_fcs else Dot15d4)(raw)
        assert packet.fcf_frametype == 2 and packet.fcf_framever == 0
        assert packet.seqnum == 0x6A
        assert packet.fcf_destaddrmode == packet.fcf_srcaddrmode == 0
        if has_fcs:
            assert packet.fcs == 0x79E4
            assert packet.compute_fcs(raw[:-2]) == raw[-2:]
        count += 1
    assert count == 1


def check_data(path):
    count = 0
    for linktype, raw in records(path):
        assert linktype == 195 and len(raw) == 27
        packet = Dot15d4FCS(raw, stop_dissection_after=Dot15d4Data)
        assert packet.fcf_frametype == 1 and packet.fcf_framever == 1
        assert packet.seqnum == 7 and packet.fcf_panidcompress == 1
        assert packet.fcf_destaddrmode == packet.fcf_srcaddrmode == 3
        data = packet[Dot15d4Data]
        assert data.dest_panid == 0x1234
        assert data.dest_addr == 0x0401123456789ABC
        assert data.src_addr == 0x0201123456789ABC
        assert raw[21:-2] == bytes.fromhex("deadbeef")
        assert packet.compute_fcs(raw[:-2]) == raw[-2:]
        count += 1
    assert count > 0
    return count


def check_vectors(path):
    captured = list(records(path))
    assert len(captured) == 2
    linktype, ack = captured[0]
    has_fcs = linktype == 195
    assert ack == bytes.fromhex("02006ae479" if has_fcs else "02006a")
    assert captured[1][0] == linktype
    raw = captured[1][1]
    assert len(raw) == (27 if has_fcs else 25)
    packet = (Dot15d4FCS if has_fcs else Dot15d4)(raw, stop_dissection_after=Dot15d4Data)
    assert packet.fcf_frametype == 1 and packet.fcf_framever == 1 and packet.fcf_ackreq == 1
    assert packet.seqnum == 7 and packet.fcf_panidcompress == 1
    assert packet.fcf_destaddrmode == packet.fcf_srcaddrmode == 3
    data = packet[Dot15d4Data]
    assert data.dest_panid == 0x1234
    assert data.dest_addr == 0x0401123456789ABC and data.src_addr == 0x0201123456789ABC
    assert (raw[21:-2] if has_fcs else raw[21:]) == bytes.fromhex("deadbeef")
    if has_fcs:
        assert packet.compute_fcs(raw[:-2]) == raw[-2:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ack-fcs", type=Path)
    parser.add_argument("--ack-no-fcs", type=Path)
    parser.add_argument("--data-capture", action="append", type=Path, default=[])
    parser.add_argument("--vector-capture", action="append", type=Path, default=[])
    args = parser.parse_args()
    assert args.ack_fcs or args.ack_no_fcs or args.data_capture or args.vector_capture
    print("Independent decoder: Scapy", scapy.__version__)
    # Independently construct the normative 7.2.11 ACK, including its CRC.
    assert bytes(Dot15d4FCS(fcf_frametype=2, fcf_framever=0,
                          fcf_destaddrmode=0, fcf_srcaddrmode=0,
                          seqnum=0x6A)) == bytes.fromhex("02006ae479")
    for path, has_fcs in ((args.ack_fcs, True), (args.ack_no_fcs, False)):
        if path:
            check_ack(path, has_fcs)
            print("PASS ACK", path, hashlib.sha256(path.read_bytes()).hexdigest())
    for path in args.data_capture:
        count = check_data(path)
        print("PASS DATA", count, path, hashlib.sha256(path.read_bytes()).hexdigest())
    for path in args.vector_capture:
        check_vectors(path)
        print("PASS VECTORS", path, hashlib.sha256(path.read_bytes()).hexdigest())


if __name__ == "__main__":
    main()
