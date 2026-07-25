"""Discriminated PHY-profile dispatcher."""

from collections.abc import Mapping

from . import eht, he, ht, legacy, vht


def decode_phy_observation(fields: Mapping[str, object]):
    for profile in (eht, he, vht, ht):
        if profile.matches(fields):
            return profile.decode(fields)
    return legacy.decode(fields)


__all__ = ["decode_phy_observation"]

