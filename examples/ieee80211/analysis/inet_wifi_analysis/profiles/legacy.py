"""Legacy OFDM/DSSS radiotap profile."""

from collections.abc import Mapping

from ..observation import PhyObservation
from .common import first, pick


def decode(fields: Mapping[str, object]) -> PhyObservation:
    rate = first(pick(fields, "radiotap.datarate"))
    limitations = () if rate else ("legacy data rate is unavailable",)
    return PhyObservation(
        generation="legacy",
        ppdu_format="Legacy",
        bandwidth_mhz=None,
        authoritative_fields=frozenset(),
        limitations=limitations + ("legacy channel width is unavailable",),
        raw={"datarate_mbps": rate or None},
    )
