"""HT radiotap profile."""

from collections.abc import Mapping

from ..observation import PhyObservation
from .common import boolean, integer, pick


def matches(fields: Mapping[str, object]) -> bool:
    return boolean(pick(fields, "radiotap.present.mcs"))


def decode(fields: Mapping[str, object]) -> PhyObservation:
    mcs = integer(pick(fields, "radiotap.mcs.index"))
    bandwidth_code = integer(pick(fields, "radiotap.mcs.bw"))
    gi_code = integer(pick(fields, "radiotap.mcs.gi"))
    coding_code = integer(pick(fields, "radiotap.mcs.fec"))
    known = {
        name
        for name, value in {
            "mcs": mcs,
            "bandwidth_mhz": bandwidth_code,
            "guard_interval_us": gi_code,
            "coding": coding_code,
        }.items()
        if value is not None
    }
    return PhyObservation(
        generation="ht",
        ppdu_format="HT",
        mcs=mcs,
        bandwidth_mhz=40 if bandwidth_code == 1 else 20
        if bandwidth_code is not None
        else None,
        guard_interval_us=0.4 if gi_code == 1 else 0.8
        if gi_code is not None
        else None,
        nss=(mcs // 8) + 1 if mcs is not None else None,
        coding="LDPC" if coding_code == 1 else "BCC"
        if coding_code == 0
        else None,
        authoritative_fields=frozenset(known | ({"nss"} if mcs is not None else set())),
    )

