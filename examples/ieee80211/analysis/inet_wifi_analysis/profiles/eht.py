"""EHT radiotap profile with fail-closed known-bit handling."""

from collections.abc import Mapping

from ..observation import PhyObservation
from .common import boolean, first, integer, pick


EHT_GI_KNOWN = 0x00000004
U_SIG_PHY_VERSION_KNOWN = 0x00000001
U_SIG_BANDWIDTH_KNOWN = 0x00000002
USER_MCS_KNOWN = 0x00000002
USER_CODING_KNOWN = 0x00000004
USER_NSS_KNOWN = 0x00000010
USER_DATA_FOR_USER = 0x00000080


def matches(fields: Mapping[str, object]) -> bool:
    if boolean(pick(fields, "radiotap.present.eht")):
        return True
    if first(pick(fields, "radiotap.eht.known")):
        return True
    words = str(pick(fields, "radiotap.present.word")).split(",")
    try:
        return len(words) > 1 and bool(int(words[1].strip(), 0) & 0x4)
    except ValueError:
        return False


def _integers(value: object) -> list[int]:
    values = []
    for text in str(value or "").split(","):
        try:
            values.append(int(text.strip(), 0))
        except ValueError:
            continue
    return values


def decode(fields: Mapping[str, object]) -> PhyObservation:
    known = integer(pick(fields, "radiotap.eht.known")) or 0
    u_sig_common = integer(pick(fields, "radiotap.u_sig.common")) or 0
    user_infos = _integers(pick(fields, "radiotap.eht.user_info"))
    captured_user_infos = [
        word for word in user_infos if word & USER_DATA_FOR_USER
    ]
    user_info = (
        captured_user_infos[0] if len(captured_user_infos) == 1 else None
    )

    gi_code = (
        integer(pick(fields, "radiotap.eht.data_0.gi"))
        if known & EHT_GI_KNOWN
        else None
    )
    bandwidth_code = (
        (u_sig_common & 0x00038000) >> 15
        if u_sig_common & U_SIG_BANDWIDTH_KNOWN
        else None
    )
    mcs = (
        (user_info >> 20) & 0xF
        if user_info is not None and user_info & USER_MCS_KNOWN
        else None
    )
    coding_code = (
        (user_info >> 19) & 0x1
        if user_info is not None and user_info & USER_CODING_KNOWN
        else None
    )
    nss_code = (
        (user_info >> 24) & 0xF
        if user_info is not None and user_info & USER_NSS_KNOWN
        else None
    )
    authoritative = set()
    if u_sig_common & U_SIG_PHY_VERSION_KNOWN:
        authoritative.add("phy_version")
    if bandwidth_code in {0, 1, 2, 3, 4, 5}:
        authoritative.add("bandwidth_mhz")
    if gi_code in {0, 1, 2}:
        authoritative.add("guard_interval_us")
    if mcs is not None:
        authoritative.add("mcs")
    if coding_code in {0, 1}:
        authoritative.add("coding")
    if nss_code is not None:
        authoritative.add("nss")

    limitations = []
    if bandwidth_code is None:
        limitations.append("EHT bandwidth is not known from U-SIG")
    if not captured_user_infos:
        limitations.append("no EHT user entry is marked as the captured user")
    elif len(captured_user_infos) > 1:
        limitations.append("multiple EHT user entries are marked as the captured user")
    if mcs is None or nss_code is None:
        limitations.append("EHT per-user MCS/NSS is incomplete")

    return PhyObservation(
        generation="eht",
        ppdu_format="EHT",
        mcs=mcs,
        bandwidth_mhz={0: 20, 1: 40, 2: 80, 3: 160, 4: 320, 5: 320}.get(
            bandwidth_code
        ),
        guard_interval_us={0: 0.8, 1: 1.6, 2: 3.2}.get(gi_code),
        nss=nss_code + 1 if nss_code is not None else None,
        coding="LDPC" if coding_code == 1 else "BCC"
        if coding_code == 0
        else None,
        authoritative_fields=frozenset(authoritative),
        limitations=tuple(limitations),
        raw={
            "known": known,
            "u_sig_common": u_sig_common,
            "user_info": user_info,
            "user_infos": tuple(user_infos),
        },
    )
