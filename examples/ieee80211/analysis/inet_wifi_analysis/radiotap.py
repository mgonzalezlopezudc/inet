"""Minimal fail-closed radiotap parsing for typed EHT observations."""

import json
import struct
import subprocess
import tempfile
from collections.abc import Iterable, Iterator
from pathlib import Path


# (alignment, size) for fixed-size radiotap fields preceding U-SIG/EHT.
_FIXED_FIELDS = {
    0: (8, 8),
    1: (1, 1),
    2: (1, 1),
    3: (2, 4),
    4: (2, 2),
    5: (1, 1),
    6: (1, 1),
    7: (2, 2),
    8: (2, 2),
    9: (2, 2),
    10: (1, 1),
    11: (1, 1),
    12: (1, 1),
    13: (1, 1),
    14: (2, 2),
    15: (2, 2),
    16: (1, 1),
    17: (1, 1),
    18: (4, 8),
    19: (1, 3),
    20: (4, 8),
    21: (2, 12),
    22: (8, 12),
    23: (2, 12),
    24: (2, 12),
    25: (2, 6),
    26: (1, 1),
    27: (2, 4),
    32: (2, 6),
}


def _align(offset: int, alignment: int) -> int:
    return offset + (-offset % alignment)


def decode_eht_radiotap(frame: bytes) -> dict[str, str] | None:
    """Decode type-33/34 facts even when the installed TShark skips them."""
    if len(frame) < 8 or frame[0] != 0:
        return None
    header_length = struct.unpack_from("<H", frame, 2)[0]
    if header_length > len(frame) or header_length < 8:
        return None

    present_words = []
    offset = 4
    while True:
        if offset + 4 > header_length:
            return None
        word = struct.unpack_from("<I", frame, offset)[0]
        present_words.append(word)
        offset += 4
        if not word & 0x80000000:
            break
        if len(present_words) > 16:
            return None

    u_sig_common = None
    eht_offset = None
    for field_number in range(len(present_words) * 32):
        word = present_words[field_number // 32]
        bit = field_number % 32
        if not word & (1 << bit) or bit == 31:
            continue
        if bit in (29, 30) or field_number == 28:
            return None
        if field_number == 33:
            offset = _align(offset, 4)
            if offset + 12 > header_length:
                return None
            u_sig_common = struct.unpack_from("<I", frame, offset)[0]
            offset += 12
        elif field_number == 34:
            offset = _align(offset, 4)
            eht_offset = offset
            break
        else:
            layout = _FIXED_FIELDS.get(field_number)
            if layout is None:
                return None
            alignment, size = layout
            offset = _align(offset, alignment) + size
            if offset > header_length:
                return None

    if eht_offset is None:
        return None
    eht_length = header_length - eht_offset
    if eht_length < 40 or (eht_length - 40) % 4:
        return None
    known, *data = struct.unpack_from("<10I", frame, eht_offset)
    user_words = [
        struct.unpack_from("<I", frame, user_offset)[0]
        for user_offset in range(eht_offset + 40, header_length, 4)
    ]
    user_info = ",".join(hex(word) for word in user_words)
    return {
        "radiotap.present.word": ",".join(hex(word) for word in present_words),
        "radiotap.eht.known": hex(known),
        "radiotap.u_sig.common": hex(u_sig_common or 0),
        "radiotap.eht.data_0.gi": str((data[0] & 0x180) >> 7),
        "radiotap.eht.user_info": user_info,
    }


def decode_eht_ek_lines(lines: Iterable[str]) -> Iterator[tuple[int, dict[str, str]]]:
    for line in lines:
        try:
            document = json.loads(line)
        except json.JSONDecodeError:
            continue
        layers = document.get("layers", {})
        frame_layer = layers.get("frame", {})
        frame_number = (
            frame_layer.get("frame_frame_number")
            if isinstance(frame_layer, dict)
            else None
        )
        frame_raw = layers.get("frame_raw")
        if not frame_number or not frame_raw:
            continue
        try:
            decoded = decode_eht_radiotap(bytes.fromhex(frame_raw))
        except ValueError:
            continue
        if decoded is not None:
            yield int(frame_number), decoded


def extract_eht_radiotap(path: str | Path) -> dict[int, dict[str, str]]:
    command = ["tshark", "-n", "-r", str(path), "-T", "ek", "-x"]
    with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as errors:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=errors,
            text=True,
        )
        assert process.stdout is not None
        result = dict(decode_eht_ek_lines(process.stdout))
        returncode = process.wait()
        if returncode:
            errors.seek(0)
            raise RuntimeError(
                f"TShark raw radiotap export failed for {path}: "
                f"exit {returncode}: {errors.read().strip()}"
            )
        return result
