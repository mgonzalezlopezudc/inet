"""Canonical filesystem layout for IEEE 802.11 analysis result sessions."""

from pathlib import Path


def result_root(repository_root: str | Path, ini: str | Path) -> Path:
    """Return the example-local results directory containing an INI file."""
    repository_root = Path(repository_root)
    ini = Path(ini)
    if not ini.is_absolute():
        ini = repository_root / ini
    return ini.parent / "results"


def result_session_directory(
    repository_root: str | Path,
    ini: str | Path,
    session_id: str,
) -> Path:
    """Return results/<session-id> for the simulation example containing an INI."""
    return result_root(repository_root, ini) / session_id


def result_configuration_directory(
    repository_root: str | Path,
    ini: str | Path,
    session_id: str,
    config: str,
) -> Path:
    """Return results/<session-id>/<configuration> for a simulation run."""
    return result_session_directory(repository_root, ini, session_id) / config
