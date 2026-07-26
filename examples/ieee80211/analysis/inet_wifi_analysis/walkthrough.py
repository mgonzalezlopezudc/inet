"""Shared walkthrough ownership and results-session metadata helpers."""

from __future__ import annotations

import re

from .io import SESSION_ID_PATTERN


SCRIPT_SESSIONS_BEGIN = "<!-- BEGIN SCRIPT RESULTS SESSIONS -->"
SCRIPT_SESSIONS_END = "<!-- END SCRIPT RESULTS SESSIONS -->"
SCRIPT_SESSION_FAMILIES = ("Scalar/vector", "PCAP")
OWNERSHIP_PREFIX_PATTERN = re.compile(
    r"^\[(?:agent|script)\]\s+",
    re.IGNORECASE,
)


def normalize_heading_label(label: str) -> str:
    """Return a formatting- and ownership-neutral heading label."""
    normalized = re.sub(r"[`*_]", "", label)
    normalized = OWNERSHIP_PREFIX_PATTERN.sub("", normalized.strip())
    return re.sub(r"\s+", " ", normalized).strip().lower()


def _script_session_block(sessions: dict[str, str]) -> str:
    lines = [
        SCRIPT_SESSIONS_BEGIN,
        "`[script]` results sessions:",
        "",
    ]
    lines.extend(
        f"- {family}: `{sessions.get(family, 'NOT RUN')}`"
        for family in SCRIPT_SESSION_FAMILIES
    )
    lines.append(SCRIPT_SESSIONS_END)
    return "\n".join(lines)


def _parse_script_sessions(content: str) -> dict[str, str]:
    sessions = {family: "NOT RUN" for family in SCRIPT_SESSION_FAMILIES}
    if SCRIPT_SESSIONS_BEGIN not in content:
        if SCRIPT_SESSIONS_END in content:
            raise ValueError("Malformed script results-session markers")
        return sessions
    if (
        content.count(SCRIPT_SESSIONS_BEGIN) != 1
        or content.count(SCRIPT_SESSIONS_END) != 1
    ):
        raise ValueError("Malformed script results-session markers")
    begin = content.index(SCRIPT_SESSIONS_BEGIN)
    end = content.index(SCRIPT_SESSIONS_END, begin)
    block = content[begin:end]
    for family in SCRIPT_SESSION_FAMILIES:
        matches = re.findall(
            rf"^- {re.escape(family)}:\s+`([^`]+)`\s*$",
            block,
            re.MULTILINE,
        )
        if len(matches) != 1:
            raise ValueError(
                f"Script results-session ledger must contain exactly one "
                f"{family} entry"
            )
        values = [value.strip() for value in matches[0].split(",")]
        if not values or any(
            value != "NOT RUN"
            and SESSION_ID_PATTERN.fullmatch(value) is None
            for value in values
        ):
            raise ValueError(
                f"Invalid {family} results-session value: {matches[0]}"
            )
        sessions[family] = matches[0]
    return sessions


def update_script_results_session(
    content: str,
    family: str,
    session_id: str,
) -> str:
    """Update one script-owned session entry without changing agent metadata."""
    if family not in SCRIPT_SESSION_FAMILIES:
        raise ValueError(f"Unknown script results-session family: {family}")
    if not SESSION_ID_PATTERN.fullmatch(session_id):
        raise ValueError(f"Invalid results session ID: {session_id}")
    sessions = _parse_script_sessions(content)
    sessions[family] = session_id
    block = _script_session_block(sessions)

    if SCRIPT_SESSIONS_BEGIN in content:
        begin = content.index(SCRIPT_SESSIONS_BEGIN)
        end = content.index(SCRIPT_SESSIONS_END, begin) + len(
            SCRIPT_SESSIONS_END
        )
        return content[:begin] + block + content[end:]

    title = re.search(r"^#\s+.+$", content, re.MULTILINE)
    if title is None:
        raise ValueError("Walkthrough has no level-one title")
    insertion = title.end()
    agent_label = "`[agent]` results sessions:"
    agent_occurrences = content.count(agent_label)
    if agent_occurrences > 1:
        raise ValueError("Walkthrough has multiple agent results-session lines")
    inserted = block + "\n\n"
    if agent_occurrences == 0:
        inserted += (
            "`[agent]` results sessions: `NOT RECORDED`.\n\n"
        )
    return (
        content[:insertion].rstrip()
        + "\n\n"
        + inserted
        + content[insertion:].lstrip("\n")
    )
