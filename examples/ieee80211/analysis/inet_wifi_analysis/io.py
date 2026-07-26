"""Small shared filesystem and identifier helpers for analysis tools."""

import os
import re
import tempfile
from pathlib import Path


SESSION_ID_PATTERN = re.compile(r"^\d{8}T\d{6}Z$")


def atomic_write_text(path: Path, content: str) -> None:
    """Replace a text artifact only after its complete content is durable."""
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        dir=path.parent, prefix=f".{path.name}.", suffix=".tmp", text=True
    )
    temporary_path = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
    except Exception:
        temporary_path.unlink(missing_ok=True)
        raise
