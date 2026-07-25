"""Typed PHY observations shared by packet and walkthrough analysis."""

from dataclasses import asdict, dataclass, field
from typing import Any


@dataclass(frozen=True)
class PhyObservation:
    generation: str
    ppdu_format: str | None = None
    mcs: int | None = None
    bandwidth_mhz: int | None = None
    ru: str | None = None
    guard_interval_us: float | None = None
    nss: int | None = None
    coding: str | None = None
    authoritative_fields: frozenset[str] = field(default_factory=frozenset)
    limitations: tuple[str, ...] = ()
    raw: dict[str, Any] = field(default_factory=dict, compare=False)

    def to_dict(self) -> dict[str, Any]:
        value = asdict(self)
        value["authoritative_fields"] = sorted(self.authoritative_fields)
        return value

    @property
    def bandwidth_or_ru(self) -> str | None:
        if self.ru is not None:
            return self.ru
        return (
            f"{self.bandwidth_mhz} MHz"
            if self.bandwidth_mhz is not None
            else None
        )

