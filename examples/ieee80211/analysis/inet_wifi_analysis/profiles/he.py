"""HE radiotap profile."""

from collections.abc import Mapping

from ..observation import PhyObservation
from .common import boolean, integer, pick


FORMAT = {0: "HE-SU", 1: "HE-ER-SU", 2: "HE-MU", 3: "HE-TB"}
BANDWIDTH_OR_RU = {
    0: (20, None),
    1: (40, None),
    2: (80, None),
    3: (160, None),
    4: (None, "26-tone RU"),
    5: (None, "52-tone RU"),
    6: (None, "106-tone RU"),
    7: (None, "242-tone RU"),
    8: (None, "484-tone RU"),
    9: (None, "996-tone RU"),
    10: (None, "2x996-tone RU"),
}


def matches(fields: Mapping[str, object]) -> bool:
    return boolean(pick(fields, "radiotap.present.he"))


def decode(fields: Mapping[str, object]) -> PhyObservation:
    fmt = integer(pick(fields, "radiotap.he.data_1.ppdu_format"))
    mcs = integer(pick(fields, "radiotap.he.data_3.data_mcs"))
    coding_code = integer(pick(fields, "radiotap.he.data_3.coding"))
    bw_ru_code = integer(
        pick(fields, "radiotap.he.data_5.data_bw_ru_allocation")
    )
    gi_code = integer(pick(fields, "radiotap.he.data_5.gi"))
    nss = integer(pick(fields, "radiotap.he.data_6.nsts"))
    bandwidth, ru = BANDWIDTH_OR_RU.get(bw_ru_code, (None, None))
    values = {
        "ppdu_format": fmt,
        "mcs": mcs,
        "bandwidth_or_ru": bw_ru_code,
        "guard_interval_us": gi_code,
        "nss": nss,
        "coding": coding_code,
    }
    return PhyObservation(
        generation="he",
        ppdu_format=FORMAT.get(fmt, "HE"),
        mcs=mcs,
        bandwidth_mhz=bandwidth,
        ru=ru,
        guard_interval_us={0: 0.8, 1: 1.6, 2: 3.2}.get(gi_code),
        nss=nss if nss is not None and nss > 0 else None,
        coding="LDPC" if coding_code == 1 else "BCC"
        if coding_code == 0
        else None,
        authoritative_fields=frozenset(
            name for name, value in values.items() if value is not None
        ),
    )

