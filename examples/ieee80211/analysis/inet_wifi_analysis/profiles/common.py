"""Shared profile parsing helpers."""

from collections.abc import Mapping


TRUE_VALUES = {"1", "true", "yes"}


def first(value: object) -> str:
    if value is None:
        return ""
    return str(value).split(",", 1)[0].strip()


def integer(value: object) -> int | None:
    text = first(value)
    if not text:
        return None
    try:
        return int(text, 0)
    except ValueError:
        return None


def boolean(value: object) -> bool:
    return first(value).lower() in TRUE_VALUES


def pick(fields: Mapping[str, object], name: str) -> object:
    return fields.get(name, "")

