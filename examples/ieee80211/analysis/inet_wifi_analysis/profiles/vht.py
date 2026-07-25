"""VHT radiotap profile."""

from collections.abc import Mapping

from ..observation import PhyObservation
from .common import boolean, integer, pick


def matches(fields: Mapping[str, object]) -> bool:
    return boolean(pick(fields, "radiotap.present.vht"))


def decode(fields: Mapping[str, object]) -> PhyObservation:
    mcs = integer(pick(fields, "radiotap.vht.mcs.0"))
    nss = integer(pick(fields, "radiotap.vht.nss.0"))
    bandwidth_code = integer(pick(fields, "radiotap.vht.bw"))
    gi_code = integer(pick(fields, "radiotap.vht.gi"))
    coding_code = integer(pick(fields, "radiotap.vht.coding.0"))
    bandwidth = {0: 20, 1: 40, 4: 80, 11: 160}.get(bandwidth_code)
    values = {
        "mcs": mcs,
        "nss": nss,
        "bandwidth_mhz": bandwidth,
        "guard_interval_us": gi_code,
        "coding": coding_code,
    }
    return PhyObservation(
        generation="vht",
        ppdu_format="VHT",
        mcs=mcs,
        bandwidth_mhz=bandwidth,
        guard_interval_us=0.4 if gi_code == 1 else 0.8
        if gi_code == 0
        else None,
        nss=nss,
        coding="LDPC" if coding_code == 1 else "BCC"
        if coding_code == 0
        else None,
        authoritative_fields=frozenset(
            name for name, value in values.items() if value is not None
        ),
    )

