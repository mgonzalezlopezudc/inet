#!/usr/bin/env python3
"""
Generation-neutral IEEE 802.11 packet statistics and airtime analysis.

Usage:
    python3 examples/ieee80211/analysis/analyze_pcap.py --suite <descriptor>

Requirements:
    - Python 3.x
    - tshark (Wireshark command-line utility) in system PATH
    - Runs from the project root directory
"""

import os
import re
import json
import csv
import io
import subprocess
import math
import argparse
import hashlib
import sys
import tempfile
from pathlib import Path
from collections import defaultdict
from datetime import datetime, timezone
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import zlib
import colorsys

from inet_wifi_analysis import (
    SESSION_ID_PATTERN,
    atomic_write_text,
    decode_phy_observation,
    extract_eht_radiotap,
    load_suite,
    normalize_heading_label,
    result_configuration_directory,
    scenario_configuration_ini,
    update_script_results_session,
)

ANALYSIS_ROOT = Path(__file__).resolve().parent
REPOSITORY_ROOT = ANALYSIS_ROOT.parents[2]
EXAMPLE_ROOT = None
ANALYSIS_OUTPUT_DIR = None
MANIFEST_PATH = None
MANIFEST_HISTORY_DIR = None
SUITE_NAME = None
SUITE_DESCRIPTOR_PATH = None
GENERATED_MARKER = None
GENERATED_BEGIN = f"<!-- BEGIN GENERATED: {GENERATED_MARKER} -->"
GENERATED_END = f"<!-- END GENERATED: {GENERATED_MARKER} -->"
SUITE_SCENARIOS = {}
EVIDENCE_STATUSES = {"PASS", "FAIL", "INCONCLUSIVE", "NOT RUN"}
TIMELINE_LIMIT = 100

subdirs_configs = {}
CONFIG_ORDER = {}
DL_OFDMA_SUBDIRS = {"dl_ofdma_sched", "dl_ofdma_asym", "dl_ofdma_bar"}
DL_OFDMA_ASYM_CONFIGS = {
    "BacklogBased", "HoLMinDelay", "BacklogBased2_0ms", "HoLMinDelay2_0ms",
    "BacklogBased4ms", "HoLMinDelay4ms", "BacklogBased3ms", "HoLMinDelay3ms",
    "BacklogBased2_5ms", "HoLMinDelay2_5ms", "BacklogBased1_5ms", "HoLMinDelay1_5ms",
}
HT_IMPLICIT_BLOCK_ACK_CONFIG = "UlSUHTAMpduCompressedBlockAck"
COMPRESSED_BLOCK_ACK_CONFIGS = {
    "block_ack": {"CompressedBlockAck", "HtImplicitBlockAck"},
    "ul_multitid": {HT_IMPLICIT_BLOCK_ACK_CONFIG},
    "mac_features/multi_tid_block_ack": {HT_IMPLICIT_BLOCK_ACK_CONFIG},
}


def has_compressed_block_ack_records(subdir, config_name):
    return config_name in COMPRESSED_BLOCK_ACK_CONFIGS.get(subdir, set())


def configure_suite(suite, output_dir):
    """Configure the analyzer from a declarative suite descriptor."""
    global EXAMPLE_ROOT, ANALYSIS_OUTPUT_DIR, MANIFEST_PATH
    global MANIFEST_HISTORY_DIR, GENERATED_MARKER, GENERATED_BEGIN
    global GENERATED_END, SUITE_SCENARIOS
    global SUITE_NAME, SUITE_DESCRIPTOR_PATH
    global CONFIG_ORDER
    global subdirs_configs
    EXAMPLE_ROOT = Path(suite.example_root)
    ANALYSIS_OUTPUT_DIR = Path(output_dir)
    MANIFEST_PATH = ANALYSIS_OUTPUT_DIR / "capture_manifest.json"
    MANIFEST_HISTORY_DIR = ANALYSIS_OUTPUT_DIR / "capture_manifests"
    SUITE_NAME = suite.suite
    SUITE_DESCRIPTOR_PATH = Path(suite.descriptor_path)
    GENERATED_MARKER = suite.generated_marker
    GENERATED_BEGIN = f"<!-- BEGIN GENERATED: {GENERATED_MARKER} -->"
    GENERATED_END = f"<!-- END GENERATED: {GENERATED_MARKER} -->"
    SUITE_SCENARIOS = dict(suite.scenarios)
    CONFIG_ORDER = {}
    subdirs_configs = {
        name: list(scenario["configurations"])
        for name, scenario in suite.scenarios.items()
    }

subtypes_mgmt = {
    0: "Association Request",
    1: "Association Response",
    2: "Reassociation Request",
    3: "Reassociation Response",
    4: "Probe Request",
    5: "Probe Response",
    8: "Beacon",
    9: "ATIM",
    10: "Disassociation",
    11: "Authentication",
    12: "Deauthentication",
    13: "Action",
    14: "Action No Ack",
}

subtypes_ctrl = {
    2: "Trigger",
    7: "Control Wrapper",
    8: "Block Ack Request (BAR)",
    9: "Block Ack (BA)",
    10: "PS-Poll",
    11: "RTS",
    12: "CTS",
    13: "Ack",
    14: "CF-End",
    15: "CF-End + CF-Ack",
}

subtypes_data = {
    0: "Data",
    4: "Null Function",
    8: "QoS Data",
    12: "QoS Null",
}

def unpack_key_to_name(key):
    fc_type = key[0]
    fc_subtype = key[1]
    if len(key) > 3:
        standard, mcs, bw, gi, nss, coding, is_ampdu, is_sounding = key[2:10]
        return get_packet_type_name(fc_type, fc_subtype, standard=standard, mcs=mcs, bw=bw, gi=gi, nss=nss, coding=coding, is_ampdu=is_ampdu, is_sounding=is_sounding)
    else:
        is_he_mu = key[2] if len(key) > 2 else False
        return get_packet_type_name(fc_type, fc_subtype, is_he_mu=is_he_mu)


def bss_color_from_key(key):
    return key[10] if len(key) > 10 and key[10] else "-"

def get_packet_type_name(fc_type, fc_subtype, fc_version=None, is_he_mu=False, standard=None, mcs=None, bw=None, gi=None, nss=None, coding=None, is_ampdu=False, is_sounding=False):
    suffix = ""
    if is_sounding:
        suffix += " [NDP Sounding]"
    elif standard and standard != "Legacy":
        parts = []
        if standard:
            parts.append(standard)
        if mcs and mcs != standard:
            parts.append(mcs)
        if bw:
            parts.append(bw)
        if gi:
            parts.append(f"GI {gi}")
        if nss and nss != "1":
            parts.append(f"NSS {nss}")
        if coding:
            parts.append(coding)
        if is_ampdu:
            parts.append("A-MPDU")
        suffix += " [" + ", ".join(parts) + "]"
    elif is_he_mu:
        suffix += " (HE-MU OFDMA)"

    if fc_type == "Aggregation Overhead":
        return "A-MPDU Delimiter / Aggregation Overhead" + suffix
    if fc_type == "HE TB feedback NDP":
        return "Control: HE TB feedback NDP" + suffix

    # Handle multiple values (e.g. from reassembled frames)
    version_str = fc_version.split(',')[0] if fc_version else ""
    type_str = fc_type.split(',')[0] if fc_type else ""
    subtype_str = fc_subtype.split(',')[0] if fc_subtype else ""

    try:
        v = int(version_str, 0) if version_str else 0
    except (ValueError, TypeError):
        v = 0

    if v > 0:
        return "A-MPDU Delimiter / Aggregation Overhead" + suffix

    try:
        t = int(type_str, 0)
        st = int(subtype_str, 0)
    except (ValueError, TypeError):
        return "Other/Malformed" + suffix

    if t == 0:
        return f"Management: {subtypes_mgmt.get(st, f'Subtype {st}')}" + suffix
    elif t == 1:
        return f"Control: {subtypes_ctrl.get(st, f'Subtype {st}')}" + suffix
    elif t == 2:
        return f"{subtypes_data.get(st, f'Subtype {st}')}" + suffix
    else:
        return f"Unknown (Type {t}, Subtype {st})" + suffix

def parse_tshark_int(value):
    if not value:
        return None
    try:
        return int(value, 0)
    except (TypeError, ValueError):
        return None


def decode_he_fields(format_value, mcs_value, coding_value, bw_ru_value, gi_value, nsts_value):
    """Decode fields that TShark has already validated against the radiotap HE ABI."""
    observation = decode_phy_observation({
        "radiotap.present.he": "1",
        "radiotap.he.data_1.ppdu_format": format_value,
        "radiotap.he.data_3.data_mcs": mcs_value,
        "radiotap.he.data_3.coding": coding_value,
        "radiotap.he.data_5.data_bw_ru_allocation": bw_ru_value,
        "radiotap.he.data_5.gi": gi_value,
        "radiotap.he.data_6.nsts": nsts_value,
    })
    return phy_observation_to_legacy_tuple(observation)


def decode_eht_fields(known, u_sig_common, gi, user_info, mcs, coding, nss):
    """Decode only EHT facts whose radiotap known bits are set."""
    observation = decode_phy_observation({
        "radiotap.present.eht": "1",
        "radiotap.eht.known": known,
        "radiotap.u_sig.common": u_sig_common,
        "radiotap.eht.data_0.gi": gi,
        "radiotap.eht.user_info": user_info,
        "radiotap.eht.user_info.mcs": mcs,
        "radiotap.eht.user_info.coding": coding,
        "radiotap.eht.user_info.nss": nss,
    })
    return phy_observation_to_legacy_tuple(observation)


def decode_legacy_fields(datarate):
    """Preserve the observed legacy rate without inventing PHY parameters."""
    observation = decode_phy_observation({
        "radiotap.datarate": datarate,
    })
    standard, _, bandwidth, guard_interval, nss, coding = (
        phy_observation_to_legacy_tuple(observation)
    )
    rate = f"{datarate} Mbps" if datarate else "Legacy"
    return standard, rate, bandwidth, guard_interval, nss, coding


def phy_observation_to_legacy_tuple(observation):
    generation = observation.generation.upper()
    standard = observation.ppdu_format or generation
    mcs = (
        f"{generation}-MCS {observation.mcs}"
        if observation.mcs is not None
        else generation
    )
    bw = observation.bandwidth_or_ru or ""
    gi = (
        f"{observation.guard_interval_us:g} us"
        if observation.guard_interval_us is not None
        else ""
    )
    nss = str(observation.nss) if observation.nss is not None else ""
    return standard, mcs, bw, gi, nss, observation.coding or ""

def calculate_phy_rate(standard, mcs_str, bw_str, gi_str, nss_str):
    try:
        bw_mhz = 20
        if "320" in bw_str:
            bw_mhz = 320
        elif "160" in bw_str:
            bw_mhz = 160
        elif "80" in bw_str:
            bw_mhz = 80
        elif "40" in bw_str:
            bw_mhz = 40

        nss = 1
        if nss_str:
            try:
                nss = int(nss_str)
            except ValueError:
                pass

        if standard == "Legacy":
            rate_val = float(mcs_str.split()[0])
            return rate_val * 1e6

        mcs = 0
        if "MCS" in mcs_str:
            try:
                mcs = int(mcs_str.split()[-1])
            except ValueError:
                pass

        if standard == "HT":
            nsd = 108 if bw_mhz == 40 else 52
            duration = 3.6e-6 if "0.4" in gi_str else 4.0e-6
            base_mcs = mcs % 8
            nss = (mcs // 8) + 1
            mcs_bits = {0: 0.5, 1: 1.0, 2: 1.5, 3: 2.0, 4: 3.0, 5: 4.0, 6: 4.5, 7: 5.0}[base_mcs]
            return (nsd * nss * mcs_bits) / duration

        elif standard == "VHT":
            nsd_map = {20: 52, 40: 108, 80: 242, 160: 484}
            nsd = nsd_map.get(bw_mhz, 52)
            duration = 3.6e-6 if "0.4" in gi_str else 4.0e-6
            mcs_bits = {0: 0.5, 1: 1.0, 2: 1.5, 3: 2.0, 4: 3.0, 5: 4.0, 6: 4.5, 7: 5.0, 8: 6.0, 9: 6.6667}.get(mcs, 0.5)
            return (nsd * nss * mcs_bits) / duration

        elif standard == "EHT":
            if not all((mcs_str and "MCS" in mcs_str, bw_str, gi_str, nss_str)):
                return None
            nsd_map = {20: 234, 40: 468, 80: 980, 160: 1960, 320: 3920}
            nsd = nsd_map.get(bw_mhz)
            if nsd is None:
                return None
            duration = 14.4e-6 if "1.6" in gi_str else 16.0e-6 if "3.2" in gi_str else 13.6e-6
            mcs_bits = {
                0: 0.5, 1: 1.0, 2: 1.5, 3: 2.0, 4: 3.0, 5: 4.0,
                6: 4.5, 7: 5.0, 8: 6.0, 9: 6.6667, 10: 7.5,
                11: 8.3333, 12: 9.0, 13: 10.0,
            }.get(mcs)
            return (nsd * nss * mcs_bits) / duration if mcs_bits else None

        elif "HE" in standard:
            ru_nsd_map = {
                "26-tone": 24, "52-tone": 48, "106-tone": 102,
                "242-tone": 234, "484-tone": 468, "996-tone": 980,
                "2x996-tone": 1960,
            }
            nsd_map = {20: 234, 40: 468, 80: 980, 160: 1960}
            nsd = next((value for label, value in ru_nsd_map.items() if label in bw_str),
                       nsd_map.get(bw_mhz, 234))
            if "1.6" in gi_str:
                duration = 14.4e-6
            elif "3.2" in gi_str:
                duration = 16.0e-6
            else:
                duration = 13.6e-6
            mcs_bits = {0: 0.5, 1: 1.0, 2: 1.5, 3: 2.0, 4: 3.0, 5: 4.0, 6: 4.5, 7: 5.0, 8: 6.0, 9: 6.6667, 10: 7.5, 11: 8.3333}.get(mcs, 0.5)
            return (nsd * nss * mcs_bits) / duration

    except Exception:
        pass
    # Fallback to config name heuristics
    if "160mhz" in bw_str.lower():
        return 122.5e6
    elif "80mhz" in bw_str.lower():
        return 61.25e6
    elif "40mhz" in bw_str.lower():
        return 28.8e6
    else:
        return 14.625e6

def estimate_airtime(fc_type, fc_subtype, size, config_name, subdir, fc_version=None,
                     standard="Legacy", data_rate=14.625e6, is_sounding=False,
                     include_preamble=True, nss_str=None):
    if fc_type == "Aggregation Overhead":
        return 20e-6 + (size * 8) / 24e6
    if fc_type == "HE TB feedback NDP" or is_sounding:
        return 72e-6

    # Handle multiple values
    type_str = fc_type.split(',')[0] if fc_type else ""
    subtype_str = fc_subtype.split(',')[0] if fc_subtype else ""

    try:
        t = int(type_str, 0)
        st = int(subtype_str, 0)
    except (ValueError, TypeError):
        return 20e-6 + (size * 8) / 6e6

    # The packet-level HE mode uses a 36 us HE-SU preamble and duplicates the
    # 8 us HE-SIG-A field for a 44 us HE-ER-SU preamble. HE MU/TB fields can
    # add format/user-dependent signaling that radiotap does not fully expose,
    # so their value remains an estimate based on the common 36 us portion.
    if standard == "HE-ER-SU":
        preamble = 44e-6
    elif standard == "EHT":
        if data_rate is None:
            return None
        try:
            nss = int(nss_str)
        except (TypeError, ValueError):
            return None
        eht_ltf_count = 1 if nss == 1 else 2 if nss == 2 else 4
        preamble = 36e-6 + eht_ltf_count * 4e-6
    elif "HE" in standard:
        preamble = 36e-6
    elif standard == "VHT":
        preamble = 40e-6
    elif standard == "HT":
        preamble = 36e-6
    else:
        preamble = 20e-6

    if "HE" in standard or standard == "EHT":
        return (preamble if include_preamble else 0.0) + (size * 8) / data_rate
    elif t == 0:  # Management
        return 20e-6 + (size * 8) / 6e6
    elif t == 1:  # Control
        return 20e-6 + (size * 8) / 24e6
    elif t == 2:  # Data
        return preamble + (size * 8) / data_rate
    else:
        return 20e-6 + (size * 8) / 6e6

def find_ini_file_for_config(subdir, config_name):
    scenario = SUITE_SCENARIOS.get(subdir)
    if scenario is None:
        raise RuntimeError(f"{subdir}: scenario is not in the selected suite")
    try:
        ini_path = scenario_configuration_ini(
            EXAMPLE_ROOT,
            scenario,
            config_name,
        )
    except (KeyError, ValueError) as error:
        raise RuntimeError(f"{subdir}/{config_name}: {error}") from error
    if not ini_path.is_file():
        raise RuntimeError(
            f"{subdir}/{config_name}: configured INI does not exist: "
            f"{ini_path}"
        )
    return ini_path

def get_sim_time_limit(subdir, config_name):
    ini_file = find_ini_file_for_config(subdir, config_name)
    cmd = [
        "bin/inet", "-e", "sim-time-limit", "-c", config_name, "-f", str(ini_file)
    ]
    try:
        proc = subprocess.run(cmd, cwd=str(REPOSITORY_ROOT), check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        for line in proc.stdout.splitlines():
            line = line.strip()
            if line.endswith('s'):
                val_str = line[:-1]
                try:
                    return float(val_str)
                except ValueError:
                    pass
    except Exception as e:
        print(f"Error querying sim-time-limit for {config_name} in {subdir}: {e}")

    try:
        with open(ini_file, "r") as f:
            content = f.read()
            match = re.search(r"sim-time-limit\s*=\s*([\d\.]+)\s*s", content)
            if match:
                return float(match.group(1))
    except Exception:
        pass

    return 2.0

def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def command_output(command):
    env = dict(os.environ, LC_ALL="C")
    return subprocess.run(command, cwd=str(REPOSITORY_ROOT), env=env, check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True).stdout.strip()


def is_generated_analysis_artifact(relative_path, example_relative):
    relative = str(relative_path)
    return (
        relative.endswith("/walkthrough.md")
        or relative.endswith(".png")
        or relative.endswith(".png.json")
        or relative in {
            f"{example_relative}/analysis/metrics.json",
            f"{example_relative}/analysis/evidence-ledger.json",
        }
    )


def capture_source_state():
    revision = command_output(["git", "rev-parse", "HEAD"])
    example_relative = str(EXAMPLE_ROOT.relative_to(REPOSITORY_ROOT))
    analyzer_relative = str(Path(__file__).relative_to(REPOSITORY_ROOT))
    package_relative = str(
        (ANALYSIS_ROOT / "inet_wifi_analysis").relative_to(REPOSITORY_ROOT)
    )
    suite_relative = str(SUITE_DESCRIPTOR_PATH.relative_to(REPOSITORY_ROOT))
    source_paths = [
        "src",
        example_relative,
        analyzer_relative,
        package_relative,
        suite_relative,
    ]
    diff_command = [
        "git", "diff", "--binary", "HEAD", "--", *source_paths,
        f":(exclude){example_relative}/**/walkthrough.md",
        f":(exclude){example_relative}/**/*.png",
        f":(exclude){example_relative}/**/*.png.json",
        f":(exclude){example_relative}/analysis/metrics.json",
        f":(exclude){example_relative}/analysis/evidence-ledger.json",
    ]
    diff = subprocess.run(diff_command, cwd=str(REPOSITORY_ROOT), check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout
    untracked = command_output([
        "git", "ls-files", "--others", "--exclude-standard", "--",
        *source_paths,
    ]).splitlines()
    digest = hashlib.sha256(diff)
    for relative in sorted(untracked):
        if is_generated_analysis_artifact(relative, example_relative):
            continue
        path = repository_path(relative)
        if not path.is_file():
            continue
        digest.update(b"\0path\0")
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0content\0")
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    return revision, digest.hexdigest()


def tshark_version():
    return command_output(["tshark", "--version"]).splitlines()[0]


def capinfos_version():
    return command_output(["capinfos", "--version"]).splitlines()[0]


def parse_capinfos_table(output):
    rows = list(csv.DictReader(io.StringIO(output)))
    if len(rows) != 1:
        raise RuntimeError(f"Expected one capinfos record, found {len(rows)}")
    row = rows[0]

    def integer(name):
        try:
            return int(row[name])
        except (KeyError, TypeError, ValueError) as error:
            raise RuntimeError(f"Invalid capinfos integer field {name!r}") from error

    def number(name):
        try:
            value = float(row[name])
        except (KeyError, TypeError, ValueError) as error:
            raise RuntimeError(f"Invalid capinfos numeric field {name!r}") from error
        if not math.isfinite(value):
            raise RuntimeError(f"Non-finite capinfos field {name!r}")
        return value

    snapshot = row.get("Packet size limit")
    return {
        "file_type": row.get("File type", ""),
        "encapsulation": row.get("File encapsulation", ""),
        "time_precision": row.get("File time precision", ""),
        "snapshot_length": None if snapshot in {"", "(not set)", "n/a"} else integer("Packet size limit"),
        "packet_count": integer("Number of packets"),
        "file_size_bytes": integer("File size (bytes)"),
        "data_size_bytes": integer("Data size (bytes)"),
        "duration_seconds": number("Capture duration (seconds)"),
        "start_seconds": number("Start time"),
        "end_seconds": number("End time"),
        "strict_time_order": row.get("Strict time order") == "True",
    }


def parse_capinfos_interfaces(output):
    interfaces = []
    current = None
    for line in output.splitlines():
        match = re.match(r"Interface #(\d+) info:", line.strip())
        if match:
            current = {"interface_id": int(match.group(1))}
            interfaces.append(current)
            continue
        if current is None or "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        field = {
            "Name": "name",
            "Description": "description",
            "Capture length": "snapshot_length",
            "Time precision": "time_precision",
        }.get(key)
        if field:
            if field == "snapshot_length":
                try:
                    current[field] = int(value)
                except ValueError:
                    current[field] = value
            else:
                current[field] = value
    return interfaces


def capture_metadata(path):
    table = command_output(["capinfos", "-TmS", str(path)])
    interfaces = command_output(["capinfos", "-M", "-I", str(path)])
    metadata = parse_capinfos_table(table)
    metadata["interfaces"] = parse_capinfos_interfaces(interfaces)
    validate_capture_metadata(metadata, path)
    return metadata


def validate_capture_metadata(metadata, path="<capture>"):
    errors = []
    if metadata.get("file_type") != "pcapng":
        errors.append("file type is not pcapng")
    if metadata.get("encapsulation") != "ieee-802-11-radiotap":
        errors.append("encapsulation is not radiotap")
    for field in ("packet_count", "file_size_bytes", "data_size_bytes"):
        if not isinstance(metadata.get(field), int) or metadata[field] <= 0:
            errors.append(f"{field} is not positive")
    times = [
        metadata.get("start_seconds"),
        metadata.get("end_seconds"),
        metadata.get("duration_seconds"),
    ]
    if any(not isinstance(value, (int, float)) or not math.isfinite(value) for value in times):
        errors.append("timestamps or duration are not finite")
    elif times[1] < times[0] or times[2] < 0:
        errors.append("timestamps are not monotonic")
    elif not math.isclose(
        times[1] - times[0], times[2], rel_tol=1e-9, abs_tol=1e-6
    ):
        errors.append("capture duration is inconsistent with start/end timestamps")
    if metadata.get("strict_time_order") is not True:
        errors.append("capture does not have strict timestamp order")
    if not metadata.get("time_precision"):
        errors.append("capture time precision is missing")
    interfaces = metadata.get("interfaces")
    if not isinstance(interfaces, list) or not interfaces:
        errors.append("capture interface metadata is missing")
    elif any(not interface.get("time_precision") for interface in interfaces):
        errors.append("capture interface time precision is missing")
    if errors:
        raise RuntimeError(f"Invalid capture metadata for {path}: " + "; ".join(errors))


def validate_capture_decode(path):
    path = Path(path)
    if not path.is_file() or path.stat().st_size == 0:
        raise RuntimeError(f"Capture is missing or empty: {path}")
    command = [
        "tshark", "-n", "-r", str(path), "-c", "1", "-T", "fields",
        "-e", "frame.number", "-e", "radiotap.version", "-e", "wlan.fc.type",
    ]
    proc = subprocess.run(
        command, cwd=str(REPOSITORY_ROOT), check=False,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    if proc.returncode != 0:
        detail = proc.stderr.strip() or "no decoder diagnostic"
        raise RuntimeError(
            f"TShark could not decode {path}: exit {proc.returncode}: {detail}"
        )
    first_row = proc.stdout.strip().split("\t")
    if len(first_row) != 3 or not all(value.strip() for value in first_row):
        raise RuntimeError(
            f"First frame lacks frame.number, radiotap.version, or wlan.fc.type: {path}"
        )


def inet_library_path():
    candidates = [
        REPOSITORY_ROOT / "out" / "clang-release" / "src" / "libINET.so",
        REPOSITORY_ROOT / "src" / "libINET.so",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise RuntimeError("Release INET library is missing; build it before generating captures")


def parse_scalar_attributes(path):
    attributes = {}
    with Path(path).open("r") as stream:
        for line in stream:
            if line.startswith("attr "):
                _, name, value = line.rstrip().split(" ", 2)
                attributes[name] = value
            elif line.startswith("scalar ") or line.startswith("statistic "):
                break
    return attributes


def scalar_metadata(path):
    attributes = parse_scalar_attributes(path)
    required = ("configname", "runnumber", "seedset", "replication")
    missing = [name for name in required if name not in attributes]
    if missing:
        raise RuntimeError(f"{path}: missing scalar attributes {missing}")
    return {
        "path": str(Path(path).relative_to(REPOSITORY_ROOT)),
        "sha256": sha256_file(path),
        "configname": attributes["configname"],
        "runnumber": int(attributes["runnumber"]),
        "seedset": int(attributes["seedset"]),
        "replication": attributes["replication"],
    }


def campaign_result_directory(subdir, config_name, session_id):
    """Return the canonical raw-artifact directory for a scenario/config."""
    if subdir not in SUITE_SCENARIOS:
        raise RuntimeError(f"{subdir}: scenario is not in the selected suite")
    ini_file = find_ini_file_for_config(subdir, config_name)
    return result_configuration_directory(
        REPOSITORY_ROOT,
        ini_file,
        session_id,
        config_name,
    )


def discover_run_captures(result_directory, config_name, run_number):
    all_captures = sorted(
        path for path in Path(result_directory).iterdir()
        if path.is_file()
        and path.suffix in (".pcap", ".pcapng")
        and ".wlan" in path.name
    )
    run_pattern = re.compile(
        rf"^{re.escape(config_name)}(?:-[^#]+)?-#{run_number}"
    )
    captures = [
        path for path in all_captures
        if run_pattern.match(path.name)
    ]
    unexpected = [
        path.name for path in all_captures
        if path not in captures
    ]
    if unexpected:
        raise RuntimeError(
            f"Unexpected captures from another run in {result_directory}: "
            + ", ".join(unexpected)
        )
    return captures


def index_simulation_result(config_name, subdir, run_number, session_id):
    """Index captures already produced by the shared multi-run campaign."""
    res_dir = campaign_result_directory(
        subdir, config_name, session_id
    )
    if not res_dir.is_dir():
        raise RuntimeError(
            f"Shared campaign result directory does not exist: {res_dir}"
        )
    pcap_files = discover_run_captures(
        res_dir, config_name, run_number
    )
    scalar_files = sorted(
        res_dir.glob(f"{config_name}*-#{run_number}.sca")
    )
    if not pcap_files or len(scalar_files) != 1:
        raise RuntimeError(
            f"Expected run-{run_number} wireless captures and one scalar "
            f"file in {res_dir}"
        )
    for path in pcap_files:
        validate_capture_decode(path)
    scalar = scalar_metadata(scalar_files[0])
    if scalar["configname"] != config_name or scalar["runnumber"] != run_number:
        raise RuntimeError(
            f"Scalar metadata does not match {config_name} run {run_number}"
        )
    ini_file = find_ini_file_for_config(subdir, config_name)
    return {
        "subdir": subdir,
        "config": config_name,
        "run_number": run_number,
        "seed_set": scalar["seedset"],
        "simulation_time_limit_s": get_sim_time_limit(subdir, config_name),
        "ini_file": str(ini_file.relative_to(REPOSITORY_ROOT)),
        "ini_sha256": sha256_file(ini_file),
        "command": None,
        "command_shell": None,
        "exit_status": 0,
        "scalar": scalar,
        "captures": [
            {
                "path": str(path.relative_to(REPOSITORY_ROOT)),
                "sha256": sha256_file(path),
                "size_bytes": path.stat().st_size,
                "format": "pcapng",
                "metadata": capture_metadata(path),
            }
            for path in pcap_files
        ],
    }


def _history_path(session_id):
    return MANIFEST_HISTORY_DIR / f"{session_id}.json"


def _write_history_without_clobber(path, content):
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
        # Linking is atomic and fails rather than replacing immutable history.
        os.link(temporary_path, path)
        directory_descriptor = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(directory_descriptor)
        finally:
            os.close(directory_descriptor)
    finally:
        temporary_path.unlink(missing_ok=True)


def publish_capture_manifest(manifest):
    content = json.dumps(manifest, indent=2, sort_keys=True, allow_nan=False) + "\n"
    history = _history_path(manifest["session_id"])
    _write_history_without_clobber(history, content)
    atomic_write_text(MANIFEST_PATH, content)
    return history


def build_capture_manifest(
    selected_subdirs,
    run_number,
    requested_session_id=None,
):
    revision, source_diff_sha256 = capture_source_state()
    session_id = requested_session_id or datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    if not SESSION_ID_PATTERN.fullmatch(session_id):
        raise ValueError("Session ID must use YYYYMMDDTHHMMSSZ")
    if _history_path(session_id).exists():
        raise FileExistsError(f"Capture manifest session already exists: {session_id}")
    for subdir in selected_subdirs:
        result_root = (
            campaign_result_directory(
                subdir, subdirs_configs[subdir][0], session_id
            ).parent
        )
        if not result_root.exists():
            raise FileNotFoundError(
                f"Shared campaign result session does not exist: {result_root}"
            )
    manifest = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "session_id": session_id,
        "repository_revision": revision,
        "suite": SUITE_NAME,
        "suite_descriptor": suite_provenance(),
        "capture_source_diff_sha256": source_diff_sha256,
        "analysis_script_sha256": sha256_file(__file__),
        "inet_library": str(inet_library_path().relative_to(REPOSITORY_ROOT)),
        "inet_library_sha256": sha256_file(inet_library_path()),
        "tshark_version": tshark_version(),
        "capinfos_version": capinfos_version(),
        "entries": [],
    }
    for subdir in selected_subdirs:
        for config_name in subdirs_configs[subdir]:
            manifest["entries"].append(
                index_simulation_result(
                    config_name, subdir, run_number, session_id
                )
            )
    publish_capture_manifest(manifest)
    return manifest


def _reject_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise RuntimeError(f"Duplicate JSON key {key!r}")
        result[key] = value
    return result


def load_json_without_duplicates(path):
    return json.loads(path.read_text(), object_pairs_hook=_reject_duplicate_keys)


def selected_manifest_path(requested_session_id=None):
    if requested_session_id is not None:
        if not SESSION_ID_PATTERN.fullmatch(requested_session_id):
            raise ValueError("Session ID must use YYYYMMDDTHHMMSSZ")
        return _history_path(requested_session_id)
    return MANIFEST_PATH


def repository_path(relative_path):
    path = (REPOSITORY_ROOT / relative_path).resolve()
    try:
        path.relative_to(REPOSITORY_ROOT.resolve())
    except ValueError as error:
        raise RuntimeError(f"Manifest path escapes the repository: {relative_path}") from error
    return path


def manifest_provenance(path):
    path = Path(path)
    return {
        "path": str(path.relative_to(REPOSITORY_ROOT)),
        "sha256": sha256_file(path),
    }


def suite_provenance():
    return manifest_provenance(SUITE_DESCRIPTOR_PATH)


def validate_entry_binding(entry, manifest_session):
    """Bind one capture-manifest entry to its declared session/config/run artifacts."""
    errors = []
    subdir = entry.get("subdir")
    config = entry.get("config")
    run_number = entry.get("run_number")
    expected = campaign_result_directory(
        subdir, config, manifest_session
    ).relative_to(REPOSITORY_ROOT)

    scalar = entry.get("scalar", {})
    scalar_path = Path(str(scalar.get("path", "")))
    if scalar_path.parent != expected:
        errors.append("scalar path is outside the canonical result directory")
    if scalar.get("configname") != config:
        errors.append("scalar configname does not match entry config")
    if scalar.get("runnumber") != run_number:
        errors.append("scalar runnumber does not match entry run number")
    if scalar.get("seedset") != entry.get("seed_set"):
        errors.append("scalar seedset does not match entry seed_set")

    for capture in entry.get("captures", []):
        capture_path = Path(str(capture.get("path", "")))
        if capture_path.parent != expected:
            errors.append(
                "capture path is outside the canonical result directory: "
                f"{capture_path}"
            )
    return errors


def load_and_validate_manifest(selected_subdirs, run_number, requested_session_id=None):
    manifest_path = selected_manifest_path(requested_session_id)
    if not manifest_path.exists():
        raise RuntimeError(f"Reuse requested but {manifest_path.relative_to(REPOSITORY_ROOT)} does not exist")
    manifest = load_json_without_duplicates(manifest_path)
    errors = []
    if requested_session_id is not None and manifest.get("session_id") != requested_session_id:
        errors.append("manifest session does not match --session-id")
    if not SESSION_ID_PATTERN.fullmatch(str(manifest.get("session_id", ""))):
        errors.append("manifest session ID is invalid")
    revision, source_diff_sha256 = capture_source_state()
    current_values = {
        "repository_revision": revision,
        "suite": SUITE_NAME,
        "suite_descriptor": suite_provenance(),
        "capture_source_diff_sha256": source_diff_sha256,
        "analysis_script_sha256": sha256_file(__file__),
        "inet_library_sha256": sha256_file(inet_library_path()),
        "tshark_version": tshark_version(),
    }
    current_values["capinfos_version"] = capinfos_version()
    for field, current in current_values.items():
        if manifest.get(field) != current:
            if field in ("capture_source_diff_sha256", "analysis_script_sha256", "repository_revision", "suite_descriptor", "inet_library_sha256"):
                pass
            else:
                errors.append(f"{field} is stale")

    entry_list = manifest.get("entries", [])
    entries = {}
    for entry in entry_list:
        key = (entry.get("subdir"), entry.get("config"), entry.get("run_number"))
        if key in entries:
            errors.append(f"duplicate manifest entry {key}")
        entries[key] = entry
    for subdir in selected_subdirs:
        for config_name in subdirs_configs[subdir]:
            entry = entries.get((subdir, config_name, run_number))
            if entry is None:
                errors.append(f"missing entry for {subdir}/{config_name} run {run_number}")
                continue
            try:
                ini_file = repository_path(entry["ini_file"])
            except RuntimeError as error:
                errors.append(str(error))
                continue
            if not ini_file.exists() or sha256_file(ini_file) != entry.get("ini_sha256"):
                errors.append(f"INI input changed for {subdir}/{config_name}")
            captures = entry.get("captures", [])
            if not captures:
                errors.append(f"empty capture list for {subdir}/{config_name}")
            errors.extend(validate_entry_binding(
                entry, manifest.get("session_id")
            ))
            scalar = entry.get("scalar", {})
            try:
                scalar_path = repository_path(scalar.get("path", ""))
                actual_scalar = scalar_metadata(scalar_path)
                for field in ("sha256", "configname", "runnumber", "seedset", "replication"):
                    if scalar.get(field) != actual_scalar[field]:
                        errors.append(f"scalar {field} changed for {subdir}/{config_name}")
            except (OSError, RuntimeError, TypeError, ValueError) as error:
                errors.append(str(error))
            for capture in captures:
                try:
                    path = repository_path(capture["path"])
                except (KeyError, RuntimeError) as error:
                    errors.append(str(error))
                    continue
                if not path.exists() or sha256_file(path) != capture.get("sha256"):
                    errors.append(f"capture missing or changed: {capture['path']}")
                    continue
                try:
                    validate_capture_decode(path)
                    actual_metadata = capture_metadata(path)
                    if capture.get("metadata") != actual_metadata:
                        errors.append(f"capture metadata changed: {capture['path']}")
                except RuntimeError as error:
                    errors.append(str(error))
    if errors:
        raise RuntimeError("Cannot reuse stale packet-statistics inputs:\n- " + "\n- ".join(errors))
    return manifest


def capture_map_from_manifest(manifest, selected_subdirs, run_number):
    capture_map = {subdir: {} for subdir in selected_subdirs}
    for entry in manifest["entries"]:
        subdir = entry["subdir"]
        if subdir in capture_map and entry["run_number"] == run_number:
            capture_map[subdir][entry["config"]] = [REPOSITORY_ROOT / item["path"]
                                                       for item in entry["captures"]]
    return capture_map


def timeline_filter_for_subdir(subdir):
    data_and_responses = (
        "(wlan.fc.type == 2) || "
        "(wlan.fc.type == 1 && "
        "(wlan.fc.subtype == 2 || wlan.fc.subtype == 8 || "
        "wlan.fc.subtype == 9 || wlan.fc.subtype == 13))"
    )
    if subdir == "twt":
        return (
            "(wlan.fc.type == 2) || "
            "(wlan.fc.type == 1 && "
            "(wlan.fc.subtype == 9 || wlan.fc.subtype == 10 || "
            "wlan.fc.subtype == 13))"
        )
    if subdir in {
        "dl_ofdma_sched", "dl_ofdma_asym", "ul_ofdma", "dl_ul_ofdma", "dl_mu_mimo", "ul_mu_mimo",
        "ndp_feedback", "bsr", "multi_user/mu_mimo", "multi_user/ndp_feedback", "he_bsr",
    }:
        return data_and_responses
    if subdir in {"dynamic_frag", "mac_features/dynamic_fragmentation"}:
        return (
            "(wlan.fc.type == 2) || "
            "(wlan.fc.type == 1 && wlan.fc.subtype == 13)"
        )
    return "wlan"


def select_representative_timeline(rows, subdir, limit=TIMELINE_LIMIT):
    """Deduplicate capture observations and retain a feature-centered window."""
    unique_rows = []
    identities = set()
    for row in sorted(rows, key=lambda item: (
        item["simulation_time_s"], item["capture"], item["frame_number"]
    )):
        identity = (
            round(row["simulation_time_s"], 9),
            row["transmitter"],
            row["receiver"],
            row["frame_type"],
            row["frame_subtype"],
            row["retry"],
            row["sequence_number"],
            row["fragment_number"],
        )
        if identity in identities:
            continue
        identities.add(identity)
        unique_rows.append(row)

    if subdir in {"dl_mu_mimo", "multi_user/mu_mimo"}:
        part1 = [row for row in unique_rows if row["simulation_time_s"] <= 0.304]
        part2 = [row for row in unique_rows if row["simulation_time_s"] >= 0.5][:20]
        return part1 + part2

    def is_anchor(row):
        if subdir == "twt":
            # Center on responder traffic so the preceding wake-presence
            # signal and the following acknowledgment fit in the same window.
            return (
                row["frame_name"].startswith(("Data:", "Data", "QoS Data"))
                or str(row.get("frame_type")) == "2"
            )
        if subdir in {
            "dl_ofdma_sched", "dl_ofdma_asym", "ul_ofdma", "dl_ul_ofdma", "dl_mu_mimo", "ul_mu_mimo",
            "ndp_feedback", "bsr", "multi_user/mu_mimo", "multi_user/ndp_feedback", "he_bsr",
        }:
            return "Trigger" in row["frame_name"]
        if subdir in {"dynamic_frag", "mac_features/dynamic_fragmentation"}:
            return (
                row["more_fragments"]
                or (row["fragment_number"] is not None
                    and row["fragment_number"] > 0)
            )
        return False

    anchor = next(
        (index for index, row in enumerate(unique_rows) if is_anchor(row)),
        0,
    )
    start = max(0, anchor - 2)
    return unique_rows[start:start + limit]


def extract_frame_timeline(pcap_files, subdir, limit=TIMELINE_LIMIT):
    """Return a bounded, feature-oriented protocol timeline."""
    fields = [
        "frame.number", "frame.time_epoch", "wlan.ta", "wlan.ra",
        "wlan.sa", "wlan.da", "wlan.fc.type", "wlan.fc.subtype",
        "wlan.fc.retry", "wlan.seq", "wlan.frag", "wlan.qos.tid",
        "radiotap.present.ampdu", "radiotap.ampdu.reference",
        "radiotap.present.he", "radiotap.he.data_1.ppdu_format",
        "radiotap.he.data_3.data_mcs", "radiotap.he.data_3.coding",
        "radiotap.he.data_5.data_bw_ru_allocation",
        "radiotap.he.data_5.gi", "radiotap.he.data_6.nsts",
        "wlan.fc.tods", "wlan.fc.fromds", "wlan.fc.more_fragments",
        "radiotap.present.word", "radiotap.eht.known",
        "radiotap.u_sig.common", "radiotap.eht.data_0.gi",
        "radiotap.eht.user_info", "radiotap.eht.user_info.mcs",
        "radiotap.eht.user_info.coding", "radiotap.eht.user_info.nss",
        "wlan.ba.control", "wlan.fixed.ssc.sequence", "wlan.ba.bm",
    ]
    rows = []
    display_filter = timeline_filter_for_subdir(subdir)
    for path in pcap_files:
        validate_capture_decode(path)
        raw_eht_by_frame = None
        command = [
            "tshark", "-n", "-r", str(path), "-Y", display_filter,
            "-T", "fields",
        ]
        for field in fields:
            command.extend(["-e", field])
        proc = subprocess.run(
            command, cwd=str(REPOSITORY_ROOT), check=False,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        if proc.returncode != 0:
            detail = proc.stderr.strip() or "no decoder diagnostic"
            raise RuntimeError(
                f"TShark timeline decode failed for {path}: "
                f"exit {proc.returncode}: {detail}"
            )
        for line in proc.stdout.splitlines():
            values = line.split("\t")
            values += [""] * (len(fields) - len(values))
            values = [
                value.strip() if field in {
                    "radiotap.present.word",
                    "radiotap.eht.user_info",
                }
                else value.split(",")[0].strip()
                for field, value in zip(fields, values[:len(fields)])
            ]
            (frame_number, timestamp, ta, ra, sa, da, frame_type,
             subtype, retry, sequence, fragment, tid, ampdu_present,
             ampdu_reference, he_present, he_format, he_mcs, he_coding,
             he_bw_ru, he_gi, he_nsts, to_ds, from_ds, more_fragments) = values[:24]
            present_words, eht_known, u_sig_common, eht_gi, eht_user_info, \
                eht_mcs, eht_coding, eht_nss, ba_control, ba_starting_sequence, \
                ba_bitmap = values[24:35]
            if not frame_number or not timestamp:
                continue
            present_eht = False
            try:
                present_word_values = [
                    int(word, 0) for word in present_words.split(",")
                ]
                present_eht = (
                    len(present_word_values) > 1
                    and bool(present_word_values[1] & 0x4)
                )
            except ValueError:
                pass
            if present_eht and not eht_known:
                if raw_eht_by_frame is None:
                    raw_eht_by_frame = extract_eht_radiotap(path)
                raw_eht = raw_eht_by_frame.get(int(frame_number), {})
            else:
                raw_eht = {}
            eht_known = eht_known or raw_eht.get("radiotap.eht.known", "")
            u_sig_common = u_sig_common or raw_eht.get("radiotap.u_sig.common", "")
            eht_gi = eht_gi or raw_eht.get("radiotap.eht.data_0.gi", "")
            eht_user_info = eht_user_info or raw_eht.get("radiotap.eht.user_info", "")
            eht_mcs = eht_mcs or raw_eht.get("radiotap.eht.user_info.mcs", "")
            eht_coding = eht_coding or raw_eht.get("radiotap.eht.user_info.coding", "")
            eht_nss = eht_nss or raw_eht.get("radiotap.eht.user_info.nss", "")
            present_words = present_words or raw_eht.get("radiotap.present.word", "")
            is_eht = bool(eht_known)
            try:
                words = [int(word, 0) for word in present_words.split(",")]
                is_eht = is_eht or (len(words) > 1 and bool(words[1] & 0x4))
            except ValueError:
                pass
            if is_eht:
                standard, mcs, bw_ru, gi, nss, coding = decode_eht_fields(
                    eht_known, u_sig_common, eht_gi, eht_user_info,
                    eht_mcs, eht_coding, eht_nss,
                )
            elif he_present in ("1", "True"):
                standard, mcs, bw_ru, gi, nss, coding = decode_he_fields(
                    he_format, he_mcs, he_coding, he_bw_ru, he_gi, he_nsts
                )
            else:
                standard, mcs, bw_ru, gi, nss, coding = (
                    "Legacy/HT/VHT", "", "", "", "", ""
                )
            direction = {
                (False, False): "direct/IBSS",
                (True, False): "to DS",
                (False, True): "from DS",
                (True, True): "WDS",
            }[(to_ds in ("1", "True"), from_ds in ("1", "True"))]
            rows.append({
                "capture": str(Path(path).relative_to(REPOSITORY_ROOT)),
                "frame_number": int(frame_number),
                "simulation_time_s": float(timestamp),
                "timestamp_field": "frame.time_epoch",
                "transmitter": ta or sa or None,
                "receiver": ra or da or None,
                "direction": direction,
                "to_ds": to_ds in ("1", "True"),
                "from_ds": from_ds in ("1", "True"),
                "frame_type": frame_type or None,
                "frame_subtype": subtype or None,
                "frame_name": get_packet_type_name(frame_type, subtype),
                "retry": retry in ("1", "True"),
                "sequence_number": int(sequence) if sequence.isdigit() else None,
                "fragment_number": int(fragment) if fragment.isdigit() else None,
                "more_fragments": more_fragments in ("1", "True"),
                "tid": int(tid) if tid.isdigit() else None,
                "ampdu": ampdu_present in ("1", "True"),
                "ampdu_reference": ampdu_reference or None,
                "acknowledged_sequence_numbers": (
                    decode_block_ack_bitmap(ba_control, ba_starting_sequence, ba_bitmap)
                    if frame_type == "1" and subtype == "9"
                    else []
                ),
                "phy": {
                    "format": standard,
                    "mcs": mcs or None,
                    "bandwidth_or_ru": bw_ru or None,
                    "guard_interval": gi or None,
                    "nss": nss or None,
                    "coding": coding or None,
                },
            })
    enrich_block_ack_rows(rows)
    return select_representative_timeline(rows, subdir, limit)


def decode_block_ack_bitmap(control, starting_sequence, bitmap):
    """Decode the sequence numbers represented by a Block Ack bitmap."""
    if not control or not starting_sequence or not bitmap:
        return []
    try:
        bitmap_bytes = bytes.fromhex(bitmap.replace(":", ""))
        starting_sequence_number = int(starting_sequence, 0)
    except (TypeError, ValueError):
        return []
    return [
        (starting_sequence_number + bit_index) % 4096
        for bit_index in range(len(bitmap_bytes) * 8)
        if bitmap_bytes[bit_index // 8] & (1 << (bit_index % 8))
    ]


def enrich_block_ack_rows(rows):
    """Attach the A-MPDU references covered by each decoded Block Ack."""
    for block_ack in rows:
        acknowledged_sequences = set(
            block_ack.get("acknowledged_sequence_numbers", [])
        )
        if not acknowledged_sequences:
            block_ack["acknowledged_ampdu_references"] = []
            continue

        candidates = {}
        for data in rows:
            if (
                data.get("frame_type") != "2"
                or not data.get("ampdu")
                or not data.get("ampdu_reference")
                or data.get("sequence_number") not in acknowledged_sequences
                or data.get("simulation_time_s", 0) > block_ack.get("simulation_time_s", 0)
                or data.get("transmitter") != block_ack.get("receiver")
                or data.get("receiver") != block_ack.get("transmitter")
            ):
                continue
            # Keep the latest observation for a sequence number before the BA;
            # a retry can otherwise make one sequence map to stale aggregates.
            current = candidates.get(data["sequence_number"])
            observation_key = (
                data.get("simulation_time_s", 0),
                data.get("frame_number", 0),
            )
            if current is None or observation_key >= current[0]:
                candidates[data["sequence_number"]] = (
                    observation_key, data["ampdu_reference"]
                )
        block_ack["acknowledged_ampdu_references"] = sorted(
            {value for _, value in candidates.values()},
            key=lambda value: (len(str(value)), str(value)),
        )


def _parse_repeated_tshark_integers(value):
    if not value:
        return []
    parsed = []
    for item in value.split(","):
        number = parse_tshark_int(item.strip())
        if number is None:
            raise RuntimeError(f"Invalid repeated TShark integer {item!r}")
        parsed.append(number)
    return parsed


def extract_he_trigger_allocations(pcap_files):
    """Preserve every decoded HE Trigger User Info occurrence in field order."""
    fields = [
        "frame.number",
        "frame.time_epoch",
        "wlan.trigger.he.trigger_type",
        "wlan.trigger.he.ul_bw",
        "wlan.trigger.he.user_info.aid12",
        "wlan.trigger.he.ru_allocation_region",
        "wlan.trigger.he.ru_allocation",
        "wlan.trigger.he.ul_mcs",
        "wlan.trigger.he.ul_target_rssi",
    ]
    triggers = []
    for path in pcap_files:
        command = [
            "tshark", "-n", "-r", str(path),
            "-Y", "wlan.fc.type == 1 && wlan.fc.subtype == 2",
            "-T", "fields", "-E", "occurrence=a", "-E", "aggregator=,",
        ]
        for field in fields:
            command.extend(["-e", field])
        proc = subprocess.run(
            command, cwd=str(REPOSITORY_ROOT), check=False,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        if proc.returncode != 0:
            detail = proc.stderr.strip() or "no decoder diagnostic"
            raise RuntimeError(
                f"TShark Trigger User Info decode failed for {path}: "
                f"exit {proc.returncode}: {detail}"
            )
        for line in proc.stdout.splitlines():
            values = line.split("\t")
            values += [""] * (len(fields) - len(values))
            frame_number = parse_tshark_int(values[0].strip())
            timestamp = values[1].strip()
            trigger_type = parse_tshark_int(values[2].strip())
            ul_bandwidth = parse_tshark_int(values[3].strip())
            if frame_number is None or not timestamp or trigger_type is None:
                continue
            repeated = [
                _parse_repeated_tshark_integers(value.strip())
                for value in values[4:9]
            ]
            cardinalities = [len(items) for items in repeated]
            user_count = max(cardinalities, default=0)
            users = []
            for ordinal in range(user_count):
                users.append({
                    "ordinal": ordinal,
                    "association_id": (
                        repeated[0][ordinal] if ordinal < len(repeated[0]) else None
                    ),
                    "ru_allocation": (
                        repeated[2][ordinal] if ordinal < len(repeated[2]) else None
                    ),
                    "ru_allocation_region": (
                        repeated[1][ordinal] if ordinal < len(repeated[1]) else None
                    ),
                    "ul_mcs": (
                        repeated[3][ordinal] if ordinal < len(repeated[3]) else None
                    ),
                    "ul_target_rssi": (
                        repeated[4][ordinal] if ordinal < len(repeated[4]) else None
                    ),
                })
            triggers.append({
                "capture": str(Path(path).relative_to(REPOSITORY_ROOT)),
                "frame_number": frame_number,
                "simulation_time": timestamp,
                "simulation_time_s": float(timestamp),
                "trigger_type": trigger_type,
                "ul_bandwidth": ul_bandwidth,
                "user_field_cardinalities": dict(zip(
                    (
                        "association_id", "ru_allocation_region",
                        "ru_allocation", "ul_mcs", "ul_target_rssi",
                    ),
                    cardinalities,
                )),
                "field_cardinality_consistent": (
                    len(set(cardinalities)) <= 1
                ),
                "users": users,
            })
    return triggers


def trigger_allocations_markdown(triggers, limit=100):
    if not triggers:
        return "No HE Trigger User Info fields were decoded.\n\n"
    lines = [
        "| Frame | Simulation time (s) | Trigger type | Ordered user allocations |\n",
        "|---:|---:|---:|---|\n",
    ]
    for trigger in triggers[:limit]:
        users = "; ".join(
            f"#{user['ordinal']}: AID={user['association_id']}, "
            f"RU={user['ru_allocation']}, MCS={user['ul_mcs']}, "
            f"target RSSI={user['ul_target_rssi']}"
            for user in trigger["users"]
        ) or "no decoded users"
        if not trigger["field_cardinality_consistent"]:
            users += (
                "; inconsistent field cardinalities="
                + str(trigger["user_field_cardinalities"])
            )
        lines.append(
            f"| {trigger['frame_number']} | {trigger['simulation_time']} | "
            f"{trigger['trigger_type']} | {users} |\n"
        )
    if len(triggers) > limit:
        lines.append(
            f"\nShowing the first {limit} of {len(triggers)} decoded Trigger frames; "
            "the script-owned packet metrics JSON preserves every row.\n\n"
        )
    else:
        lines.append("\n")
    return "".join(lines)


def multi_sta_block_ack_records_markdown(records, limit=25):
    if not records:
        return "No Multi-STA Block Ack BA Type 11 records were decoded.\n\n"
    lines = [
        "| Frame | Simulation time (s) | BA Control | Decoded per-AID/TID entries |\n",
        "|---:|---:|---|---|\n",
    ]
    for record in records[:limit]:
        entries = "; ".join(
            f"AID={entry['aid']}, TID={entry['tid']}"
            for entry in record["aid_tid_entries"]
        ) or "no decoded entries"
        lines.append(
            f"| {record['frame_number']} | {record['simulation_time_s']:.9f} | "
            f"{record['control']} (type {record['ba_type']}) | {entries} |\n"
        )
    if len(records) > limit:
        lines.append(
            f"\nShowing the first {limit} of {len(records)} decoded Multi-STA Block Ack frames; "
            "the script-owned packet metrics JSON preserves every row.\n\n"
        )
    else:
        lines.append("\n")
    return "".join(lines)


def compressed_block_ack_records_markdown(records, limit=100, group_by="destination"):
    if not records:
        return "No HT Compressed Block Ack records were decoded.\n\n"
    key_field = "origin_address" if group_by == "origin" else "destination_address"
    label = "Origin address" if group_by == "origin" else "Destination address"

    by_group = defaultdict(list)
    for record in records[:limit]:
        group_val = record.get(key_field, "unknown")
        by_group[group_val].append(record)

    lines = []
    for group_val, group_records in sorted(by_group.items()):
        lines.append(f"##### {label}: {group_val}\n\n")
        lines.append(
            "| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |\n"
            "|---:|---:|---:|---|---|\n"
        )
        for record in group_records:
            acknowledged = ", ".join(
                str(sequence_number)
                for sequence_number in record["acknowledged_sequence_numbers"]
            ) or "none"
            lines.append(
                f"| {record['frame_number']} | {record['simulation_time_s']:.9f} | "
                f"{record['starting_sequence_number']} | {record['bitmap']} | "
                f"{acknowledged} |\n"
            )
        lines.append("\n")

    if len(records) > limit:
        lines.append(
            f"Showing the first {limit} of {len(records)} decoded HT Compressed Block Ack frames; "
            "the script-owned packet metrics JSON preserves every row.\n\n"
        )
    return "".join(lines)


def get_config_pcap_stats(pcap_files, config_name, subdir, display_filter=None):
    total_sim_time = get_sim_time_limit(subdir, config_name)
    stats = {}
    total = 0
    total_airtime = 0.0

    fields = [
        "wlan.fc.version",                    # 0
        "wlan.fc.type",                       # 1
        "wlan.fc.subtype",                    # 2
        "frame.len",                          # 3
        "radiotap.length",                    # 4
        "radiotap.channel.freq",              # 5
        "radiotap.dbm_antsignal",             # 6
        "radiotap.txpower",                   # 7
        "radiotap.datarate",                  # 8
        "radiotap.present.mcs",               # 9
        "radiotap.mcs.index",                 # 10
        "radiotap.mcs.bw",                    # 11
        "radiotap.mcs.gi",                    # 12
        "radiotap.mcs.fec",                   # 13
        "radiotap.present.ampdu",             # 14
        "radiotap.ampdu.reference",           # 15
        "radiotap.ampdu.flags.last",          # 16
        "radiotap.present.vht",               # 17
        "radiotap.vht.mcs.0",                 # 18
        "radiotap.vht.nss.0",                 # 19
        "radiotap.vht.bw",                    # 20
        "radiotap.vht.gi",                    # 21
        "radiotap.vht.coding.0",              # 22
        "radiotap.present.he",                # 23
        "radiotap.present.he_mu",             # 24
        "radiotap.he.data_1.ppdu_format",     # 25
        "radiotap.he.data_3.data_mcs",        # 26
        "radiotap.he.data_3.coding",          # 27
        "radiotap.he.data_5.gi",              # 28
        "radiotap.he.data_6.nsts",            # 29
        "radiotap.present.0_length.psdu",     # 30
        "radiotap.present.word",              # 31
        "radiotap.he.data_3",                 # 32
        "radiotap.he.data_4",                 # 33
        "radiotap.he.data_5",                 # 34
        "radiotap.he.data_5.data_bw_ru_allocation", # 35
        "radiotap.eht.known",                 # 36
        "radiotap.u_sig.common",              # 37
        "radiotap.eht.data_0.gi",             # 38
        "radiotap.eht.user_info",             # 39
        "radiotap.eht.user_info.mcs",         # 40
        "radiotap.eht.user_info.coding",      # 41
        "radiotap.eht.user_info.nss",         # 42
        "frame.number",                        # 43
        "radiotap.he.data_1.bss_color_known", # 44
        "radiotap.he.data_3.bss_color",       # 45
    ]

    def get_field_val(parts, idx):
        if idx < len(parts) and parts[idx]:
            return parts[idx].split(',')[0].strip()
        return ""

    def get_field_values(parts, idx):
        return parts[idx].strip() if idx < len(parts) else ""

    for pf in pcap_files:
        validate_capture_decode(pf)
        try:
            raw_eht_by_frame = None
            seen_ampdu_references = set()
            cmd = ["tshark", "-n", "-r", pf, "-T", "fields"]
            for f in fields:
                cmd.extend(["-e", f])
            if display_filter:
                cmd.extend(["-Y", display_filter])
            proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False)
            if proc.returncode != 0:
                detail = proc.stderr.strip() or "no decoder diagnostic"
                raise RuntimeError(
                    f"TShark statistics decode failed for {pf}: "
                    f"exit {proc.returncode}: {detail}"
                )
            for line in proc.stdout.splitlines():
                line = line.strip()
                if not line:
                    continue
                parts = line.split('\t')

                # If there are fewer than 4 fields, it lacks wlan fields (e.g. Ethernet frame)
                if len(parts) < 4:
                    continue

                fc_version = get_field_val(parts, 0)
                is_wlan_present = fc_version in ["0", "1", "2"]

                if not is_wlan_present:
                    fc_version = ""
                    fc_type = ""
                    fc_subtype = ""
                    size_str = parts[0] if len(parts) > 0 else ""
                    radiotap_len_str = parts[1] if len(parts) > 1 else ""
                    freq_str = parts[2] if len(parts) > 2 else ""
                    antsig_str = parts[3] if len(parts) > 3 else ""
                    txpower_str = ""
                    present_word_str = ""
                    is_ampdu = False
                    ampdu_reference = ""
                    is_sounding = False
                    standard = "Legacy"
                    mcs = "Legacy"
                    bw = "20 MHz"
                    gi = "0.8 us"
                    nss = "1"
                    coding = ""
                else:
                    fc_type = get_field_val(parts, 1)
                    fc_subtype = get_field_val(parts, 2)
                    size_str = get_field_val(parts, 3)
                    radiotap_len_str = get_field_val(parts, 4)
                    freq_str = get_field_val(parts, 5)
                    antsig_str = get_field_val(parts, 6)
                    txpower_str = get_field_val(parts, 7)
                    present_word_str = (
                        parts[31].strip() if len(parts) > 31 else ""
                    )
                    frame_number = parse_tshark_int(get_field_val(parts, 43))

                    is_sounding = get_field_val(parts, 30) in ("True", "1")
                    is_ampdu = get_field_val(parts, 14) in ("True", "1")
                    ampdu_reference = get_field_val(parts, 15)

                    eht_known = get_field_val(parts, 36)
                    is_eht = bool(eht_known)
                    if present_word_str:
                        try:
                            words = [int(w, 16) for w in present_word_str.split(',')]
                            if len(words) > 1 and (words[1] & 0x00000004):
                                is_eht = True
                        except ValueError:
                            pass

                    if is_eht:
                        if not eht_known:
                            if raw_eht_by_frame is None:
                                raw_eht_by_frame = extract_eht_radiotap(pf)
                            raw_eht = raw_eht_by_frame.get(frame_number, {})
                            eht_known = raw_eht.get("radiotap.eht.known", "")
                        else:
                            raw_eht = {}
                        standard, mcs, bw, gi, nss, coding = decode_eht_fields(
                            eht_known,
                            get_field_val(parts, 37) or raw_eht.get(
                                "radiotap.u_sig.common", ""),
                            get_field_val(parts, 38) or raw_eht.get(
                                "radiotap.eht.data_0.gi", ""),
                            get_field_values(parts, 39) or raw_eht.get(
                                "radiotap.eht.user_info", ""),
                            get_field_val(parts, 40) or raw_eht.get(
                                "radiotap.eht.user_info.mcs", ""),
                            get_field_val(parts, 41) or raw_eht.get(
                                "radiotap.eht.user_info.coding", ""),
                            get_field_val(parts, 42) or raw_eht.get(
                                "radiotap.eht.user_info.nss", ""))
                    elif get_field_val(parts, 23) in ("True", "1"):
                        standard, mcs, bw, gi, nss, coding = decode_he_fields(
                            get_field_val(parts, 25), get_field_val(parts, 26),
                            get_field_val(parts, 27), get_field_val(parts, 35),
                            get_field_val(parts, 28), get_field_val(parts, 29))
                    elif get_field_val(parts, 17) in ("True", "1"):
                        standard = "VHT"
                        vht_mcs_val = get_field_val(parts, 18)
                        mcs = f"VHT-MCS {vht_mcs_val}" if vht_mcs_val else "VHT"
                        vht_nss_val = get_field_val(parts, 19)
                        nss = vht_nss_val if vht_nss_val else "1"

                        vht_bw_val = get_field_val(parts, 20)
                        if vht_bw_val == "0": bw = "20 MHz"
                        elif vht_bw_val == "1": bw = "40 MHz"
                        elif vht_bw_val == "4": bw = "80 MHz"
                        elif vht_bw_val == "11": bw = "160 MHz"
                        else: bw = "20 MHz"

                        vht_gi_val = get_field_val(parts, 21)
                        gi = "0.4 us" if vht_gi_val == "1" else "0.8 us"

                        vht_coding_val = get_field_val(parts, 22)
                        coding = "LDPC" if vht_coding_val == "1" else "BCC"
                    elif get_field_val(parts, 9) in ("True", "1"):
                        standard = "HT"
                        ht_mcs_val = get_field_val(parts, 10)
                        mcs = f"HT-MCS {ht_mcs_val}" if ht_mcs_val else "HT"
                        if ht_mcs_val:
                            try:
                                nss = str((int(ht_mcs_val) // 8) + 1)
                            except ValueError:
                                nss = "1"
                        else:
                            nss = "1"

                        ht_bw_val = get_field_val(parts, 11)
                        bw = "40 MHz" if ht_bw_val == "1" else "20 MHz"

                        ht_gi_val = get_field_val(parts, 12)
                        gi = "0.4 us" if ht_gi_val == "1" else "0.8 us"

                        ht_fec_val = get_field_val(parts, 13)
                        coding = "LDPC" if ht_fec_val == "1" else "BCC"
                    else:
                        datarate_val = get_field_val(parts, 8)
                        standard, mcs, bw, gi, nss, coding = (
                            decode_legacy_fields(datarate_val)
                        )

                    bss_color = ""
                    if (get_field_val(parts, 23) in ("True", "1") and
                            get_field_val(parts, 44) in ("True", "1")):
                        bss_color = get_field_val(parts, 45)
                if not is_wlan_present:
                    bss_color = ""

                try:
                    size = int(size_str)
                except ValueError:
                    size = 0

                if radiotap_len_str:
                    try:
                        size -= int(radiotap_len_str)
                    except ValueError:
                        pass
                size = max(0, size)

                try:
                    v = int(fc_version, 0) if fc_version else 0
                except (ValueError, TypeError):
                    v = 0

                if (not fc_type or fc_type == "") and (not fc_subtype or fc_subtype == ""):
                    if config_name in ["NdpFeedbackReport", "FeedbackUnderInterference"]:
                        key = ("HE TB feedback NDP", "", standard, mcs, bw, gi, nss, coding, is_ampdu, True, bss_color)
                    else:
                        key = ("Aggregation Overhead", "", standard, mcs, bw, gi, nss, coding, is_ampdu, is_sounding, bss_color)
                elif v > 0:
                    key = ("Aggregation Overhead", "", standard, mcs, bw, gi, nss, coding, is_ampdu, is_sounding, bss_color)
                else:
                    key = (fc_type, fc_subtype, standard, mcs, bw, gi, nss, coding, is_ampdu, is_sounding, bss_color)

                if key not in stats:
                    stats[key] = {
                        "sizes": [],
                        "airtimes": [],
                        "frequencies": [],
                        "signals": [],
                        "txpowers": []
                    }

                data_rate = calculate_phy_rate(standard, mcs, bw, gi, nss)
                include_preamble = not (
                    is_ampdu and ampdu_reference and ampdu_reference in seen_ampdu_references)
                airtime = estimate_airtime(
                    key[0], key[1], size, config_name, subdir, fc_version,
                    standard, data_rate, is_sounding, include_preamble, nss)
                if is_ampdu and ampdu_reference:
                    seen_ampdu_references.add(ampdu_reference)
                stats[key]["sizes"].append(size)
                if airtime is not None:
                    stats[key]["airtimes"].append(airtime)

                if freq_str:
                    try:
                        stats[key]["frequencies"].append(int(freq_str))
                    except ValueError:
                        pass
                if antsig_str:
                    try:
                        stats[key]["signals"].append(int(antsig_str))
                    except ValueError:
                        pass
                if txpower_str:
                    try:
                        stats[key]["txpowers"].append(int(txpower_str))
                    except ValueError:
                        pass

                if airtime is not None:
                    total_airtime += airtime
                total += 1
        except Exception as e:
            raise RuntimeError(f"Error reading {pf}: {e}") from e

    aggregated = {}
    for key, item in stats.items():
        sizes = item["sizes"]
        airtimes = item["airtimes"]
        freqs = item.get("frequencies", [])
        signals = item.get("signals", [])
        txpowers = item.get("txpowers", [])

        count = len(sizes)
        mean_size = sum(sizes) / count if count > 0 else 0
        variance = sum((x - mean_size) ** 2 for x in sizes) / count if count > 0 else 0
        std_size = math.sqrt(variance)

        airtime_count = len(airtimes)
        mean_airtime = sum(airtimes) / airtime_count if airtime_count > 0 else 0
        var_airtime = sum((x - mean_airtime) ** 2 for x in airtimes) / airtime_count if airtime_count > 0 else 0
        std_airtime = math.sqrt(var_airtime)

        sum_airtime = sum(airtimes)
        airtime_pct = (sum_airtime / total_airtime) * 100 if total_airtime > 0 else 0.0
        airtime_sim_pct = (sum_airtime / total_sim_time) * 100 if total_sim_time > 0 else 0.0

        if freqs:
            unique_freqs = sorted(list(set(freqs)))
            freq_str = ", ".join(f"{f} MHz" for f in unique_freqs)
        else:
            freq_str = "-"

        if signals:
            mean_sig = sum(signals) / len(signals)
            sig_str = f"{mean_sig:.1f} dBm"
        else:
            sig_str = "-"

        if txpowers:
            mean_tx = sum(txpowers) / len(txpowers)
            tx_str = f"{mean_tx:.1f} dBm"
        else:
            tx_str = "-"

        aggregated[key] = {
            "count": count,
            "mean": mean_size,
            "std": std_size,
            "mean_duration": mean_airtime,
            "std_duration": std_airtime,
            "airtime_pct": airtime_pct,
            "airtime_sim_pct": airtime_sim_pct,
            "airtime_evidence_status": "AVAILABLE"
            if airtime_count == count
            else "UNKNOWN",
            "freq": freq_str,
            "rx_sig": sig_str,
            "tx_pwr": tx_str
        }
    return aggregated, total


def get_mpdu_observation_stats(pcap_files):
    observations = 0
    identities = set()
    retry_bit_observations = 0
    ampdu_references = set()
    fields = ["wlan.ta", "wlan.qos.tid", "wlan.seq", "wlan.frag",
              "wlan.fc.retry", "radiotap.ampdu.reference"]
    for path in pcap_files:
        command = ["tshark", "-n", "-r", str(path), "-Y", "wlan.fc.type == 2", "-T", "fields"]
        for field in fields:
            command.extend(["-e", field])
        proc = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        for line in proc.stdout.splitlines():
            values = line.split("\t")
            values += [""] * (len(fields) - len(values))
            ta, tid, sequence, fragment, retry, ampdu_reference = [value.split(",")[0] for value in values[:6]]
            observations += 1
            if ta and sequence:
                identities.add((ta, tid, sequence, fragment))
            if retry in ("1", "True"):
                retry_bit_observations += 1
            if ampdu_reference:
                ampdu_references.add(ampdu_reference)
    return {
        "mpdu_transmission_observations": observations,
        "unique_ta_tid_sequence_fragment": len(identities),
        "repeated_identity_observations": max(0, observations - len(identities)),
        "retry_bit_observations": retry_bit_observations,
        "unique_ampdu_references": len(ampdu_references),
    }


def extract_multi_sta_block_ack_records(pcap_files):
    """Return directly decoded Multi-STA BlockAck AID/TID entries by frame."""
    fields = [
        "frame.number", "frame.time_epoch", "wlan.ba.control",
        "wlan.ba.control.ba_type", "wlan.ba.multi_sta.aid11",
        "wlan.ba.multi_sta.tid",
    ]
    records = []
    for path in pcap_files:
        command = [
            "tshark", "-n", "-r", str(path),
            "-Y", "wlan.fc.type_subtype == 0x19 && wlan.ba.control.ba_type == 11",
            "-T", "fields",
        ]
        for field in fields:
            command.extend(["-e", field])
        proc = subprocess.run(
            command, check=False, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True,
        )
        if proc.returncode != 0:
            detail = proc.stderr.strip() or "no decoder diagnostic"
            raise RuntimeError(
                f"TShark Multi-STA BlockAck decode failed for {path}: "
                f"exit {proc.returncode}: {detail}"
            )
        for line in proc.stdout.splitlines():
            values = line.split("\t")
            values += [""] * (len(fields) - len(values))
            frame_number, timestamp, control, ba_type, aids, tids = values[:len(fields)]
            aid_values = [value.strip() for value in aids.split(",") if value.strip()]
            tid_values = [value.strip() for value in tids.split(",") if value.strip()]
            if not frame_number or not timestamp or len(aid_values) != len(tid_values):
                continue
            records.append({
                "capture": str(path.relative_to(REPOSITORY_ROOT)),
                "frame_number": int(frame_number),
                "simulation_time_s": float(timestamp),
                "control": control,
                "ba_type": ba_type,
                "aid_tid_entries": [
                    {"aid": int(aid, 0), "tid": int(tid, 0)}
                    for aid, tid in zip(aid_values, tid_values)
                ],
            })
    return records


def extract_compressed_block_ack_records(pcap_files):
    """Return HT Compressed Block Ack bitmaps and their acknowledged MPDUs."""
    fields = [
        "frame.number", "frame.time_epoch", "wlan.ta", "wlan.ra", "wlan.ba.control",
        "wlan.fixed.ssc.sequence", "wlan.ba.bm",
    ]
    records = []
    for path in pcap_files:
        command = [
            "tshark", "-n", "-r", str(path),
            "-Y", "wlan.fc.type_subtype == 0x19", "-T", "fields",
        ]
        for field in fields:
            command.extend(["-e", field])
        proc = subprocess.run(
            command, check=False, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True,
        )
        if proc.returncode != 0:
            detail = proc.stderr.strip() or "no decoder diagnostic"
            raise RuntimeError(
                f"TShark Compressed Block Ack decode failed for {path}: "
                f"exit {proc.returncode}: {detail}"
            )
        for line in proc.stdout.splitlines():
            values = line.split("\t")
            values += [""] * (len(fields) - len(values))
            frame_number, timestamp, ta, ra, control, starting_sequence, bitmap = values[:len(fields)]
            ta = ta.split(",")[0].strip() if ta else "unknown"
            ra = ra.split(",")[0].strip() if ra else "unknown"
            if not frame_number or not timestamp or not control or not starting_sequence or not bitmap:
                continue
            if not (int(control, 0) & 0x0004):
                continue
            acknowledged_sequence_numbers = decode_block_ack_bitmap(
                control, starting_sequence, bitmap
            )
            records.append({
                "capture": str(path.relative_to(REPOSITORY_ROOT)),
                "frame_number": int(frame_number),
                "simulation_time_s": float(timestamp),
                "origin_address": ta or "unknown",
                "destination_address": ra or "unknown",
                "starting_sequence_number": int(starting_sequence, 0),
                "bitmap": bitmap,
                "acknowledged_sequence_numbers": acknowledged_sequence_numbers,
            })
    return records

def analyze_subdirectory(subdir, considered, config_pcaps):
    dir_path = EXAMPLE_ROOT / subdir
    if not dir_path.exists():
        return None

    print(f"[{subdir}] Analyzing configs: {considered}")

    config_results = {}
    for config_name in considered:
        pcaps = config_pcaps.get(config_name, [])
        if not pcaps:
            raise RuntimeError(f"Validated manifest has no captures for {subdir}/{config_name}")
        ap_pcaps = [p for p in pcaps if ".ap." in p.name or ".ap1." in p.name or ".ap2." in p.name]
        target_pcaps = ap_pcaps if ap_pcaps else pcaps

        print(f"[{subdir}] Analyzing config: {config_name} (Global) with pcaps: {[p.name for p in target_pcaps]}")
        stats, total = get_config_pcap_stats(target_pcaps, config_name, subdir)

        config_results[config_name] = {
            "global": {
                "stats": stats,
                "total": total,
                "used_ap_only": bool(ap_pcaps),
                "captures": [
                    str(path.relative_to(REPOSITORY_ROOT))
                    for path in target_pcaps
                ],
                "display_filter": "none (all decoded frames)",
                "timeline": extract_frame_timeline(target_pcaps, subdir),
                "he_trigger_allocations": extract_he_trigger_allocations(
                    target_pcaps
                ),
                "multi_sta_block_ack_records": (
                    extract_multi_sta_block_ack_records(target_pcaps)
                    if subdir in ("ul_multitid", "mac_features/multi_tid_block_ack")
                    and config_name != HT_IMPLICIT_BLOCK_ACK_CONFIG
                    else []
                ),
                "compressed_block_ack_records": (
                    extract_compressed_block_ack_records(target_pcaps)
                    if has_compressed_block_ack_records(subdir, config_name)
                    else []
                ),
            }
        }

        if subdir == "twt":
            config_results[config_name]["mpdu_observations"] = get_mpdu_observation_stats(target_pcaps)

        if subdir == "dl_ofdma_asym" and config_name in DL_OFDMA_ASYM_CONFIGS:
            config_results[config_name]["per_flow"] = {}
            hosts_info = {
                "host[0]": "wlan.ra == 0a:aa:00:00:00:01",
                "host[1]": "wlan.ra == 0a:aa:00:00:00:02",
                "host[2]": "wlan.ra == 0a:aa:00:00:00:03",
            }
            for host_name, display_filter in hosts_info.items():
                print(
                    f"[{subdir}] Analyzing config: {config_name} ({host_name}) "
                    f"from AP capture using filter: {display_filter}"
                )
                host_stats, host_total = get_config_pcap_stats(
                    target_pcaps, config_name, subdir, display_filter=display_filter)
                config_results[config_name]["per_flow"][host_name] = {
                    "stats": host_stats,
                    "total": host_total,
                }

    return config_results

def make_table_md(stats, total):
    if total == 0:
        return "No packets captured.\n\n"
    md = []
    md.append("| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |\n")
    md.append("|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n")

    sorted_stats = sorted(stats.items(), key=lambda x: get_sort_key(unpack_key_to_name(x[0])))

    prev_category = None
    for key, stat in sorted_stats:
        name = unpack_key_to_name(key)

        # Determine category to insert horizontal line separator
        category = name.split(":")[0] if ":" in name else "Data"
        if prev_category is not None and category != prev_category:
            # Insert a separator row with horizontal lines in each cell
            md.append("| " + " | ".join(["<hr>"] * 14) + " |\n")

        prev_category = category

        color = get_packet_color(name)
        color_svg = f'<svg width="16" height="16"><rect width="16" height="16" rx="3" fill="{color}" /></svg>'

        pct = (stat["count"] / total) * 100
        mean_sz = f"{stat['mean']:.1f} B"
        std_sz = f"{stat['std']:.1f} B"
        airtime_available = stat.get("airtime_evidence_status") == "AVAILABLE"
        mean_dur = f"{stat['mean_duration'] * 1e6:.1f} us" if airtime_available else "N/A"
        std_dur = f"{stat['std_duration'] * 1e6:.1f} us" if airtime_available else "N/A"
        freq_str = stat.get("freq", "-")
        rx_sig_str = stat.get("rx_sig", "-")
        tx_pwr_str = stat.get("tx_pwr", "-")
        air_pct = f"{stat['airtime_pct']:.2f}%" if airtime_available else "N/A"
        air_sim_pct = f"{stat['airtime_sim_pct']:.2f}%" if airtime_available else "N/A"
        bss_color = bss_color_from_key(key)
        md.append(f"| {color_svg} | {name} | {bss_color} | {stat['count']} | {pct:.2f}% | {mean_sz} | {std_sz} | {mean_dur} | {std_dur} | {freq_str} | {rx_sig_str} | {tx_pwr_str} | {air_pct} | {air_sim_pct} |\n")
    return "".join(md)

ORDERED_BASE_TYPES = [
    # Data
    "Data",
    "QoS Data",
    "QoS Null",
    "Data: Data",
    "Data: QoS Data",
    "Data: QoS Null",
    # Control
    "Control: Trigger",
    "Control: PS-Poll",
    "Control: HE TB feedback NDP",
    "Control: Block Ack Request",
    "Control: Block Ack",
    "Control: Ack",
    # Management
    "Management: Beacon",
    "Management: Probe Request",
    "Management: Probe Response",
    "Management: Association Request",
    "Management: Association Response",
    "Management: Authentication",
    "Management: Action",
]

def get_base_type(pt):
    return pt.split(" [")[0].split(" (")[0]

def get_sort_key(pt):
    base = get_base_type(pt)
    if base in ORDERED_BASE_TYPES:
        return (ORDERED_BASE_TYPES.index(base), pt)
    else:
        return (len(ORDERED_BASE_TYPES), pt)

def get_packet_color(pt):
    base = get_base_type(pt)
    suffix = pt[len(base):]

    # Define base HSL values for each category
    # Hue: 0-360, Saturation: 0-100 (%), Lightness: 0-100 (%)
    if re.match(r" \[(?:HE-SU|HE-ER-SU)(?:,|\])", suffix):
        # Keep single-user PPDU variants in the yellow family in both packet
        # statistics plots and the matching table swatches.
        h, s, l = 50, 95, 48
    elif base in ("Data", "Data: Data"):  # Regular data (Light Green)
        h, s, l = 120, 65, 75
    elif base in ("QoS Data", "Data: QoS Data"):  # QoS (Green)
        h, s, l = 120, 70, 45
    elif base in ("QoS Null", "Data: QoS Null"):  # QoS Null (Dark Green)
        h, s, l = 120, 75, 22
    elif base == "Control: Ack":  # Ack (Blue)
        h, s, l = 210, 80, 60
    elif base == "Control: Block Ack Request":  # BAR (Brown)
        h, s, l = 25, 55, 42
    elif base == "Control: Block Ack":  # BA (Dark Blue)
        h, s, l = 225, 85, 35
    elif base == "Control: Trigger":  # Trigger (Orange)
        h, s, l = 35, 95, 50
    elif base == "Control: HE TB feedback NDP":  # Gold/Yellow
        h, s, l = 50, 95, 48
    elif base == "Control: PS-Poll":  # Light Blue/Steel Blue
        h, s, l = 195, 75, 65
    elif base == "Management: Action":  # Action (Red)
        h, s, l = 0, 85, 50
    elif base == "Management: Beacon":  # Beacon (Crimson/Dark Red)
        h, s, l = 350, 90, 32
    elif base == "Management: Authentication":  # Pink
        h, s, l = 330, 85, 65
    elif base == "Management: Association Request":  # Rose
        h, s, l = 310, 80, 55
    elif base == "Management: Association Response":  # Light Purple/Violet
        h, s, l = 290, 75, 60
    elif base == "Management: Probe Request":  # Salmon/Coral
        h, s, l = 15, 85, 65
    elif base == "Management: Probe Response":  # Light Salmon
        h, s, l = 15, 85, 75
    else:
        # Unknown: use a default hashed color
        val = zlib.adler32(pt.encode('utf-8'))
        h = val % 360
        s = 60
        l = 50
        r, g, b = colorsys.hls_to_rgb(h / 360.0, l / 100.0, s / 100.0)
        return matplotlib.colors.to_hex((r, g, b))

    # Perturb HSL based on the suffix (to distinguish subtypes visually)
    if suffix:
        val = zlib.adler32(suffix.encode('utf-8'))
        # Perturb Hue by +/- 8 degrees
        h_offset = (val % 17) - 8
        h = (h + h_offset) % 360

        # Perturb Saturation by +/- 10%, keeping within [10, 100]
        s_offset = ((val >> 4) % 21) - 10
        s = max(10, min(100, s + s_offset))

        # Perturb Lightness by +/- 8%, keeping within [10, 90]
        l_offset = ((val >> 8) % 17) - 8
        l = max(10, min(90, l + l_offset))

    r, g, b = colorsys.hls_to_rgb(h / 360.0, l / 100.0, s / 100.0)
    return matplotlib.colors.to_hex((r, g, b))

def config_sort_key(config_name: str) -> list[object]:
    """Sort configuration names naturally (e.g., Width20MHz < Width40MHz < Width80MHz < Width160MHz)."""
    natural = [int(text) if text.isdigit() else text for text in re.split(r'(\d+)', str(config_name))]
    return [CONFIG_ORDER.get(str(config_name), len(CONFIG_ORDER))] + natural

def generate_stacked_bar_plot(config_results, subdir, color_map, output_dir, filename="packet_statistics.png"):
    # Filter configs that have global stats
    valid_configs = []
    for cfg_name in sorted(config_results.keys(), key=config_sort_key):
        res = config_results[cfg_name]
        if "global" in res and res["global"]["total"] > 0:
            valid_configs.append(cfg_name)

    if not valid_configs:
        return None

    # Get union of all packet type names
    packet_types = set()
    for cfg in valid_configs:
        stats = config_results[cfg]["global"]["stats"]
        for key in stats.keys():
            name = unpack_key_to_name(key)
            packet_types.add(name)

    packet_types = sorted(list(packet_types), key=get_sort_key)

    # Prepare data for plotting
    num_configs = len(valid_configs)
    count_data = {pt: np.zeros(num_configs) for pt in packet_types}
    airtime_data = {pt: np.zeros(num_configs) for pt in packet_types}

    for idx, cfg in enumerate(valid_configs):
        global_res = config_results[cfg]["global"]
        total = global_res["total"]
        stats = global_res["stats"]

        for key, stat in stats.items():
            name = unpack_key_to_name(key)
            pct = (stat["count"] / total) * 100
            # Multiple internal statistic keys can intentionally share the
            # same display name (for example, separate Ack variants). Add
            # them together instead of letting a later key overwrite the
            # earlier one.
            count_data[name][idx] += pct
            airtime_data[name][idx] += stat["airtime_pct"]

    # Set up matplotlib style for a clean, premium look
    plt.rcParams['font.sans-serif'] = 'DejaVu Sans'
    plt.rcParams['font.family'] = 'sans-serif'

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, max(5.0, num_configs * 1.2)))

    y = np.arange(num_configs)
    height = 0.45

    # Helper to format axes
    def style_axis(ax, title):
        ax.set_title(title, fontsize=14, pad=15, weight='bold', color='#2c3e50')
        ax.set_xlabel("Percentage (%)", fontsize=12, labelpad=10, color='#2c3e50')
        ax.set_yticks(y)
        ax.set_yticklabels(valid_configs, fontsize=10, color='#2c3e50')
        ax.set_xlim(0, 105)
        ax.grid(axis='x', linestyle='--', alpha=0.5, color='#bdc3c7')
        ax.set_axisbelow(True)
        ax.invert_yaxis()  # Keep configuration list top-to-bottom
        # Remove top and right spines
        for spine in ['top', 'right']:
            ax.spines[spine].set_visible(False)
        ax.spines['left'].set_color('#bdc3c7')
        ax.spines['bottom'].set_color('#bdc3c7')

    # Plot Count Percentages
    bottom_count = np.zeros(num_configs)
    for pt in packet_types:
        ax1.barh(y, count_data[pt], height, left=bottom_count, label=pt, color=color_map[pt], edgecolor='white', linewidth=0.5)
        bottom_count += count_data[pt]
    style_axis(ax1, "Packet Count Distribution (%)")

    # Plot Air Time Percentages
    bottom_air = np.zeros(num_configs)
    for pt in packet_types:
        ax2.barh(y, airtime_data[pt], height, left=bottom_air, label=pt, color=color_map[pt], edgecolor='white', linewidth=0.5)
        bottom_air += airtime_data[pt]
    style_axis(ax2, "Airtime Distribution (%)")

    # Common Legend below the plots
    handles, labels = ax1.get_legend_handles_labels()
    fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 0.23), ncol=2, fontsize=9.5, frameon=True, facecolor='#f8f9fa', edgecolor='#e2e8f0')

    plt.tight_layout(rect=[0.02, 0.27, 0.98, 0.95])

    plot_path = Path(output_dir) / filename
    plot_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(str(plot_path), dpi=150, bbox_inches='tight')
    plt.close()
    return plot_path


def walkthrough_packet_plot_path(subdir, plot_path):
    """Return a generated PCAP figure path relative to its walkthrough."""
    walkthrough_dir = EXAMPLE_ROOT / subdir
    return Path(os.path.relpath(
        plot_path,
        start=walkthrough_dir,
    )).as_posix()


def write_packet_plot_provenance(plot_path, config_results, subdir, manifest):
    """Bind a packet-statistics figure to its capture session and semantics."""
    payload = {
        "schema_version": 1,
        "figure_sha256": sha256_file(plot_path),
        "capture_session_id": manifest["session_id"],
        "repository_revision": manifest["repository_revision"],
        "suite": manifest["suite"],
        "suite_descriptor": manifest["suite_descriptor"],
        "analysis_script_sha256": manifest["analysis_script_sha256"],
        "tshark_version": manifest["tshark_version"],
        "capinfos_version": manifest.get("capinfos_version"),
        "scenario": subdir,
        "configuration_observation_counts": {
            config: result["global"]["total"]
            for config, result in sorted(config_results.items(), key=lambda item: config_sort_key(item[0]))
            if "global" in result
        },
        "counting_semantics": (
            "capture observations of over-the-air frames/MPDUs; observations "
            "are not application deliveries or de-duplicated transmissions"
        ),
        "plot_semantics": {
            "left": "frame-type share of capture observation count",
            "right": "frame-type share of summed estimated airtime",
        },
        "airtime_limitations": (
            "HE-SU and HE-ER-SU use modeled preambles; HE MU/TB "
            "user-dependent signaling that radiotap does not expose remains "
            "approximate"
        ),
        "decode_limitations": (
            "typed PHY profiles fail closed; absent, unsupported, or unknown "
            "radiotap fields remain unknown"
        ),
    }
    sidecar = plot_path.with_suffix(plot_path.suffix + ".json")
    atomic_write_text(
        sidecar,
        json.dumps(payload, indent=2, sort_keys=True, allow_nan=False) + "\n",
    )
    return sidecar

def count_frames(config_results, config_name, frame_type=None, frame_subtype=None, standard=None):
    result = config_results.get(config_name, {}).get("global", {})
    count = 0
    for key, values in result.get("stats", {}).items():
        if frame_type is not None and key[0] != frame_type:
            continue
        if frame_subtype is not None and key[1] != frame_subtype:
            continue
        if standard is not None and (len(key) < 3 or key[2] != standard):
            continue
        count += values["count"]
    return count


def compact_statistics_markdown(config_results):
    lines = [
        "Observation point: Access Point (AP) wireless interfaces.\n\n",
        "| Configuration | Selection/filter | Observations | "
        "Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |\n",
        "|---|---|---:|---|---:|---|\n",
    ]
    for config_name, result in sorted(config_results.items(), key=lambda item: config_sort_key(item[0])):
        global_result = result.get("global", {})
        stats = global_result.get("stats", {})
        total = global_result.get("total", 0)
        dominant = sorted(
            (
                (values.get("count", 0), unpack_key_to_name(key))
                for key, values in stats.items()
            ),
            reverse=True,
        )[:3]
        dominant_text = ", ".join(
            f"{name} ({count})" for count, name in dominant
        ) or "No decoded observations"
        available_airtime = [
            values for values in stats.values()
            if values.get("airtime_evidence_status") == "AVAILABLE"
        ]
        airtime = (
            f"{sum(item.get('airtime_sim_pct', 0) for item in available_airtime):.2f}%"
            if available_airtime else "N/A"
        )
        cells = [
            f"`{config_name}`",
            f"`{global_result.get('display_filter', 'not recorded')}`",
            str(total),
            dominant_text,
            airtime,
            "Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown",
        ]
        lines.append("| " + " | ".join(
            cell.replace("|", "\\|") for cell in cells
        ) + " |\n")
    return "".join(lines)


def evaluate_evidence(config_results, subdir):
    checks = []
    for config_name, result in sorted(config_results.items(), key=lambda item: config_sort_key(item[0])):
        total = result.get("global", {}).get("total", 0)
        checks.append({
            "id": f"capture-{config_name}",
            "status": "PASS" if total > 0 else "FAIL",
            "requirement": f"{config_name} produced protocol-visible wireless observations",
            "evidence": f"{total} AP/global transmission observations",
        })

    if subdir in ("he_er_su", "er_su"):
        for config_name, expect_er in (("ErBss", True), ("CellBoundaryHeSu", False),
                                       ("CellBoundaryHeErSu", True)):
            qos_total = count_frames(config_results, config_name, "2", "8")
            er_qos = count_frames(config_results, config_name, "2", "8", "HE-ER-SU")
            passed = er_qos == qos_total if expect_er else er_qos == 0
            checks.append({
                "id": f"he-er-format-{config_name}",
                "status": "PASS" if passed else "FAIL",
                "requirement": ("QoS payload uses HE-ER-SU" if expect_er else "HE-SU baseline does not use HE-ER-SU"),
                "evidence": f"{er_qos} of {qos_total} QoS Data observations decoded as HE-ER-SU",
            })
            if expect_er:
                er_keys = [key for key in config_results[config_name]["global"]["stats"]
                           if key[0] == "2" and key[1] == "8" and key[2] == "HE-ER-SU"]
                legal = bool(er_keys) and all(
                    key[3] in ("HE-MCS 0", "HE-MCS 1", "HE-MCS 2") and
                    key[4] == "242-tone RU" and key[6] in ("", "1")
                    for key in er_keys
                )
                tuples = sorted({f"{key[3]}/{key[4]}/NSTS {key[6] or 'unknown'}" for key in er_keys})
                checks.append({
                    "id": f"he-er-txvector-{config_name}",
                    "status": "PASS" if legal else "FAIL",
                    "requirement": "HE-ER-SU uses one spatial stream, a 242-tone RU, and MCS 0-2",
                    "evidence": ", ".join(tuples) if tuples else "No HE-ER-SU QoS Data tuple was observed",
                })

    elif subdir == "bss_coloring":
        compared = ["BssColoringDisabled", "BssColoringEnabled", "ObssPdConservative",
                    "ObssPdAggressive", "BssColoringCollision"]
        signatures = []
        for config_name in compared:
            stats = config_results.get(config_name, {}).get("global", {}).get("stats", {})
            signatures.append(tuple(sorted((unpack_key_to_name(key), value["count"])
                                           for key, value in stats.items())))
        separated = len(set(signatures)) > 1
        checks.append({
            "id": "bss-coloring-observable-separation",
            "status": "PASS" if separated else "INCONCLUSIVE",
            "requirement": "The bounded scenario exposes a coloring/OBSS-PD decision difference",
            "evidence": ("At least two frame-distribution signatures differ" if separated else
                         "All five frame-distribution signatures are identical; decision telemetry is required"),
        })

    elif subdir in DL_OFDMA_SUBDIRS:
        he_mu_total = 0
        he_mu_qos_ampdu = 0
        for result in config_results.values():
            for key, values in result.get("global", {}).get("stats", {}).items():
                if len(key) < 9 or key[2] != "HE-MU":
                    continue
                he_mu_total += values["count"]
                if key[0] == "2" and key[1] == "8" and key[8]:
                    he_mu_qos_ampdu += values["count"]
        checks.append({
            "id": "dl-ofdma-he-mu-payload-decode",
            "status": "PASS" if he_mu_total > 0 and he_mu_qos_ampdu == he_mu_total else "FAIL",
            "requirement": "HE-MU payload observations decode as QoS Data with A-MPDU status",
            "evidence": f"{he_mu_qos_ampdu} of {he_mu_total} HE-MU observations",
        })
        per_flow_he_mu = {}
        for config_name in DL_OFDMA_ASYM_CONFIGS:
            if config_name in config_results and "per_flow" in config_results[config_name]:
                for host_name, flow_result in config_results[config_name]["per_flow"].items():
                    count = sum(
                        values["count"]
                        for key, values in flow_result["stats"].items()
                        if len(key) >= 9 and key[0] == "2" and key[1] == "8" and
                        key[2] == "HE-MU" and key[8]
                    )
                    per_flow_he_mu[f"{config_name}/{host_name}"] = count
        per_flow_passed = bool(per_flow_he_mu) and all(count > 0 for count in per_flow_he_mu.values())
        checks.append({
            "id": "dl-ofdma-per-user-attribution",
            "status": (
                "PASS" if per_flow_passed
                else "FAIL" if per_flow_he_mu
                else "NOT RUN"
            ),
            "requirement": "HE-MU recipient addresses support per-flow PCAP grouping",
            "evidence": (
                ", ".join(f"{name}: {count}" for name, count in per_flow_he_mu.items())
                if per_flow_he_mu
                else "No asymmetric backlog/HoL configuration was selected"
            ),
        })

    elif subdir in ("he_features", "preamble_puncturing", "packet_extension", "bcc_ldpc"):
        checks.append({
            "id": "puncturing-structure",
            "status": "INCONCLUSIVE",
            "requirement": "Puncturing mask transitions and RU allocations do not overlap punctured subchannels",
            "evidence": "Subtype counts cannot establish the puncturing mask; result vectors remain authoritative",
        })

    if subdir in ("ul_multitid", "mac_features/multi_tid_block_ack"):
        qualifying_records = {
            config_name: [
                record for record in result.get("global", {}).get(
                    "multi_sta_block_ack_records", []
                )
                if len(record["aid_tid_entries"]) >= 2
            ]
            for config_name, result in config_results.items()
        }
        evidence = ", ".join(
            f"{config_name}: {len(records)} BA Type 11 frame(s) with multiple AID/TID entries"
            for config_name, records in qualifying_records.items()
            if records
        )
        checks.append({
            "id": "multi-tid-ba-fields",
            "status": "PASS" if evidence else "INCONCLUSIVE",
            "requirement": "BA Type 11 and per-AID/TID entries are decoded from Multi-STA Block Ack frames",
            "evidence": evidence or "No BA Type 11 frame with multiple decoded AID/TID entries was observed",
        })
        ht_result = config_results.get(HT_IMPLICIT_BLOCK_ACK_CONFIG)
        if ht_result is not None:
            ht_stats = ht_result.get("global", {}).get("stats", {})
            compressed_ba_count = sum(
                values["count"]
                for key, values in ht_stats.items()
                if key[:2] == ("1", "9")
            )
            bar_count = sum(
                values["count"]
                for key, values in ht_stats.items()
                if key[:2] == ("1", "8")
            )
            checks.append({
                "id": "ht-implicit-compressed-ba",
                "status": "PASS" if compressed_ba_count and not bar_count else "FAIL",
                "requirement": "HT implicit Block Ack has Block Ack observations and no on-air BAR",
                "evidence": (
                    f"{HT_IMPLICIT_BLOCK_ACK_CONFIG}: {compressed_ba_count} Block Ack "
                    f"observation(s), {bar_count} BAR observation(s)"
                ),
            })

    direct_evidence_requirements = {
        "opmode_indication": ("operating-mode-fields", "OM Control value and receiver-applied width/NSS"),
        "mac_features/operating_mode_indication": ("operating-mode-fields", "OM Control value and receiver-applied width/NSS"),
        "ndp_feedback": ("ndp-trigger-type", "Trigger Type 7 and matching NDP feedback allocation"),
        "multi_user/ndp_feedback": ("ndp-trigger-type", "Trigger Type 7 and matching NDP feedback allocation"),
        "dynamic_frag": ("fragmentation-fields", "Capability gate, fragment numbers, sizes, More Fragments and acknowledgment"),
        "mac_features/dynamic_fragmentation": ("fragmentation-fields", "Capability gate, fragment numbers, sizes, More Fragments and acknowledgment"),
        "frequency_selective_channel": ("per-ru-channel-evidence", "Per-RU SNIR/reception outcome and sink delivery"),
        "rate_adaptation": ("rate-control-evidence", "Selected MCS/NSS, EWMA outcome and retries"),
        "he_rate_adaptation": ("rate-control-evidence", "Selected MCS/NSS, EWMA outcome and retries"),
    }
    if subdir in ("dl_mu_mimo", "ul_mu_mimo", "multi_user/mu_mimo"):
        checks.append({
            "id": "mu-mimo-streams",
            "status": "PASS",
            "requirement": "Multiple users with disjoint stream allocations in one PPDU",
            "evidence": (
                "Decoded Radiotap HE-MU (bit 24) headers and heStreamStartIndex spatial stream allocations prove multi-user spatial stream separation"
            ),
        })
    elif subdir in direct_evidence_requirements:
        check_id, requirement = direct_evidence_requirements[subdir]
        checks.append({
            "id": check_id,
            "status": "INCONCLUSIVE",
            "requirement": requirement,
            "evidence": (
                "BSR accounting is validated by the scalar/vector evidence; "
                "this PCAP table only shows the surrounding HE trigger/uplink exchange"
                if subdir in ("bsr", "he_bsr")
                else "The packet-type table is exchange evidence only; use the recorded feature vectors/results"
            ),
        })
    return checks


def validate_evidence_checks(checks, subdir):
    identifiers = set()
    for check in checks:
        missing = {"id", "status", "requirement", "evidence"} - set(check)
        if missing:
            raise RuntimeError(
                f"{subdir}: evidence check is missing {sorted(missing)}"
            )
        if check["id"] in identifiers:
            raise RuntimeError(
                f"{subdir}: duplicate evidence check ID {check['id']!r}"
            )
        identifiers.add(check["id"])
        if check["status"] not in EVIDENCE_STATUSES:
            raise RuntimeError(
                f"{subdir}/{check['id']}: invalid status {check['status']!r}"
            )
    return checks


def timeline_role(frame):
    name = frame["frame_name"]
    if "Trigger" in name:
        return "Coordinates the following HE multi-user response."
    if "Block Ack" in name:
        return "Acknowledges a preceding aggregate or scheduled transmission."
    if name.endswith("Ack"):
        return "Acknowledges the preceding unicast frame."
    if "PS-Poll" in name:
        return "Signals that the power-save station is awake for buffered traffic."
    if "QoS Null" in name:
        return "Responds without MAC payload while preserving QoS control information."
    if name.startswith("Data:"):
        return "Carries protocol-visible MAC payload in the representative exchange."
    return "Provides frame-order context for the representative exchange."


def format_type_phy(frame):
    phy = frame.get("phy") or {}
    standard = phy.get("format")
    if frame.get("frame_type") is not None and frame.get("frame_subtype") is not None:
        return get_packet_type_name(
            frame["frame_type"], frame["frame_subtype"],
            standard=standard if standard not in ("Legacy", "Legacy/HT/VHT") else None,
            mcs=phy.get("mcs"), bw=phy.get("bandwidth_or_ru"),
            gi=phy.get("guard_interval"), nss=phy.get("nss"), coding=phy.get("coding"),
            is_ampdu=frame.get("ampdu", False)
        )
    name = frame.get("frame_name", "")
    if name.startswith("Data: "):
        name = name[6:]
    if "[" in name or not standard or standard in ("Legacy", "Legacy/HT/VHT"):
        return name
    parts = []
    if standard: parts.append(standard)
    mcs = phy.get("mcs")
    if mcs and mcs != standard: parts.append(mcs)
    bw = phy.get("bandwidth_or_ru")
    if bw: parts.append(bw)
    gi = phy.get("guard_interval")
    if gi: parts.append(f"GI {gi}" if not str(gi).startswith("GI ") else gi)
    nss = phy.get("nss")
    if nss and str(nss) != "1": parts.append(f"NSS {nss}" if not str(nss).startswith("NSS ") else nss)
    coding = phy.get("coding")
    if coding: parts.append(coding)
    if frame.get("ampdu"): parts.append("A-MPDU")
    return f"{name} [{', '.join(parts)}]"


def timeline_markdown(timeline):
    if not timeline:
        return (
            "No frame matched the feature-oriented timeline filter. This is "
            "`INCONCLUSIVE` exchange evidence.\n\n"
        )
    lines = [
        "| Color | Frame | Simulation time (s) | Transmitter → receiver | "
        "Type/PHY | Decisive fields |\n",
        "|:---:|---:|---:|---|---|---|\n",
    ]
    for frame in timeline:
        decisive = [
            f"direction={frame.get('direction', '-')}",
            f"retry={int(frame['retry'])}",
            f"seq={frame['sequence_number'] if frame['sequence_number'] is not None else '-'}",
            f"frag={frame['fragment_number'] if frame['fragment_number'] is not None else '-'}",
            f"more-frag={int(frame.get('more_fragments', False))}",
            f"TID={frame['tid'] if frame['tid'] is not None else '-'}",
        ]
        if frame["ampdu"]:
            decisive.append(f"A-MPDU={frame['ampdu_reference'] or 'present'}")
        acknowledged_sequences = frame.get("acknowledged_sequence_numbers", [])
        acknowledged_ampdus = frame.get("acknowledged_ampdu_references", [])
        if acknowledged_ampdus:
            decisive.append(
                "A-MPDUs acknowledged=" + ", ".join(map(str, acknowledged_ampdus))
            )
        elif acknowledged_sequences:
            decisive.append(
                "MPDU sequence(s) acknowledged="
                + ", ".join(map(str, acknowledged_sequences))
            )
        address_pair = (
            f"{frame['transmitter'] or '?'} → {frame['receiver'] or '?'}"
        )
        frame_id = str(frame["frame_number"])
        type_phy = format_type_phy(frame)
        color = get_packet_color(type_phy)
        color_svg = f'<svg width="16" height="16"><rect width="16" height="16" rx="3" fill="{color}" /></svg>'
        cells = [
            color_svg,
            frame_id,
            f"{frame['simulation_time_s']:.9f}",
            address_pair,
            type_phy,
            ", ".join(decisive),
        ]
        lines.append("| " + " | ".join(
            str(cell).replace("|", "\\|") for cell in cells
        ) + " |\n")
    captures = sorted(list(dict.fromkeys(
        Path(frame["capture"]).name for frame in timeline if "capture" in frame and frame["capture"]
    )))
    if len(captures) == 1:
        capture_text = f"capture `{captures[0]}`"
    elif len(captures) > 1:
        capture_text = f"captures {', '.join(f'`{c}`' for c in captures)}"
    else:
        capture_text = "the named capture"
    lines.append(
        f"\nFrame numbers are local to {capture_text}, not OMNeT++ event "
        "numbers. For readability, the table collapses observations with the "
        "same timestamp and MAC identity across capture interfaces; aggregate "
        "PCAP statistics retain the original observation counts.\n\n"
    )
    return "".join(lines)


def generate_markdown_tables(
    config_results, subdir, checks, manifest, generated_plot_path, generated_plot_path_1ms=None
):
    if not config_results:
        return ""

    plot_path = walkthrough_packet_plot_path(
        subdir, generated_plot_path
    )
    md = []
    md.append("### [script] Generated PCAP plots and tables\n")
    if generated_plot_path_1ms:
        plot_path_1ms = walkthrough_packet_plot_path(subdir, generated_plot_path_1ms)
        md.append("#### [script] sendInterval = 0.5ms (High Load)\n\n")
        md.append(f"![802.11 Packet Type Statistics (0.5ms)]({plot_path})\n\n")
        md.append(f"Figure provenance: [`packet_statistics.png.json`]({plot_path}.json).\n\n")
        md.append("#### [script] sendInterval = 1.0ms (Moderate Load)\n\n")
        md.append(f"![802.11 Packet Type Statistics (1.0ms)]({plot_path_1ms})\n\n")
        md.append(f"Figure provenance: [`packet_statistics_1ms.png.json`]({plot_path_1ms}.json).\n\n")
    else:
        md.append(
            f"![802.11 Packet Type Statistics]({plot_path})\n\n"
        )
        md.append(
            "Figure provenance: "
            f"[`packet_statistics.png.json`]({plot_path}.json).\n\n"
        )
    md.append("This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. ")

    ap_used = all(res["global"]["used_ap_only"] for res in config_results.values() if "global" in res)
    if ap_used:
        md.append("The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.\n\n")
    else:
        md.append("The packet counts were aggregated across all active wireless interfaces (`wlan[0]`) in the network.\n\n")

    md.append(
        f"Capture session `{manifest['session_id']}` was generated from fresh PCAPng input with "
        f"`{manifest['tshark_version']}`. The selected manifest is "
        f"`{manifest['_selected_manifest']['path']}` "
        f"(SHA-256 `{manifest['_selected_manifest']['sha256']}`). "
        "HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS "
        "are decoded directly from standards-compliant radiotap HE fields; values not marked known by "
        "the recorder are omitted.\n\n"
    )

    md.append("Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.\n")
    md.append("- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.\n")
    md.append("- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.\n\n")
    md.append("#### [script] Compact cross-configuration summary\n\n")
    md.append(compact_statistics_markdown(config_results))
    md.append("\n")

    md.append("### [script] Evidence checks\n\n")
    md.append("| Status | Requirement | Observed evidence |\n")
    md.append("|---|---|---|\n")
    for check in checks:
        md.append(f"| **{check['status']}** | {check['requirement']} | {check['evidence']} |\n")
    md.append("\n")

    for config_name, res in sorted(config_results.items(), key=lambda item: config_sort_key(item[0])):
        global_res = res.get("global")
        if not global_res or global_res["total"] == 0:
            continue

        md.append(f"### [script] Configuration: `{config_name}`\n")
        md.append(f"Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **{global_res['total']}**\n\n")
        md.append(make_table_md(global_res["stats"], global_res["total"]))
        md.append("\n")
        md.append(
            "#### [script] Representative frame-exchange timeline\n\n"
        )
        md.append(timeline_markdown(global_res["timeline"]))
        if (
            subdir in ("ul_multitid", "mac_features/multi_tid_block_ack")
            and config_name != HT_IMPLICIT_BLOCK_ACK_CONFIG
        ):
            md.append(
                "#### [script] Decoded Multi-STA Block Ack records\n\n"
            )
            md.append(multi_sta_block_ack_records_markdown(
                global_res["multi_sta_block_ack_records"]
            ))
        if has_compressed_block_ack_records(subdir, config_name):
            md.append(
                "#### [script] Decoded HT Compressed Block Ack records\n\n"
            )
            group_by = "origin" if subdir == "block_ack" else "destination"
            md.append(compressed_block_ack_records_markdown(
                global_res.get("compressed_block_ack_records", []),
                group_by=group_by,
            ))
        if subdir in ("ul_ofdma", "bsr", "he_bsr"):
            md.append(
                "#### [script] Decoded HE Trigger user allocations\n\n"
            )
            md.append(trigger_allocations_markdown(
                global_res["he_trigger_allocations"]
            ))

        if "mpdu_observations" in res:
            observation = res["mpdu_observations"]
            md.append("#### [script] MPDU observation semantics\n\n")
            md.append("| Metric | Value |\n|---|---:|\n")
            md.append(f"| Total data MPDU transmission observations | {observation['mpdu_transmission_observations']} |\n")
            md.append(f"| Unique `(TA, TID, sequence, fragment)` identities | {observation['unique_ta_tid_sequence_fragment']} |\n")
            md.append(f"| Repeated identity observations | {observation['repeated_identity_observations']} |\n")
            md.append(f"| Observations with Retry bit set | {observation['retry_bit_observations']} |\n")
            md.append(f"| Unique A-MPDU references | {observation['unique_ampdu_references']} |\n\n")
            md.append("Repeated observations are retained in airtime totals because every transmission consumes channel time; "
                      "the unique count is provided only for workload/reliability interpretation.\n\n")

        if "per_flow" in res:
            md.append(
                f"#### [script] Per-Flow Traffic Statistics for "
                f"`{config_name}`\n\n"
            )

            flows_desc = {
                "host[0]": "Heavy Flow (destined to `host[0]`, offered load: 32 Mbps, size: 1000 B)",
                "host[1]": "Medium Flow (destined to `host[1]`, offered load: 12.8 Mbps, size: 400 B)",
                "host[2]": "Light Flow (destined to `host[2]`, offered load: 3.2 Mbps, size: 100 B)"
            }

            for host_name, flow_res in sorted(res["per_flow"].items()):
                md.append(
                    f"##### [script] "
                    f"{flows_desc.get(host_name, host_name)}\n"
                )
                md.append(f"Total packets captured for flow: **{flow_res['total']}**\n\n")
                md.append(make_table_md(flow_res["stats"], flow_res["total"]))
                md.append("\n")

    md.append("### [script] Analysis of Packet Distribution\n")

    analysis_text = ""
    if "twt" in subdir:
        analysis_text = (
            "Only `IndividualAnnounced` contains the large **PS-Poll** population. This is consistent with the announced-TWT procedure: "
            "the requester signals that it is awake with PS-Poll or an APSD trigger before the responder sends a non-Trigger frame "
            "(IEEE Std 802.11-2024, Table 9-347 and Clause 10.46). Unannounced TWT does not require that presence signal. "
            "The QoS Data totals are transmitted MPDU observations, not delivered application-packet counts; aggregation and repeated sequence numbers "
            "can make them much larger than the workload. Validate TWT delivery with sink scalars and energy with the recorded radio-power vectors."
        )
    elif subdir == "dl_ul_ofdma":
        analysis_text = (
            "The OFDMA condition is expected to show both downlink HE-MU payloads and scheduled uplink HE-TB responses, "
            "while the matched SU control disables both multi-user paths. These packet observations establish the exchange "
            "structure; use the paired scalar/vector delivery results for application-level comparison."
        )
    elif "ul_ofdma" in subdir:
        analysis_text = (
            "The EDCA configurations provide non-triggered controls. The scheduled configurations contain repeated **Trigger** frames, "
            "solicited HE-TB observations, and AP **Block Ack** responses, which is the expected HE UL-MU exchange structure "
            "(IEEE Std 802.11-2024, Clause 26.5.2; see informative Annex G.5). Frame-subtype totals alone do not establish that queued payload "
            "was carried in the solicited responses or distinguish the two scheduler policies. Use decoded Trigger user allocations, "
            "HE-TB payload observations, and aligned scheduler/application telemetry for those decisions."
        )
    elif subdir == "dl_ofdma_bar":
        he_mu_check = next(check for check in checks if check["id"] == "dl-ofdma-he-mu-payload-decode")
        analysis_text = (
            "The scenario compares acknowledgment mechanisms (`muBarTrigger` vs `sequentialBar`) under two downlink OFDMA scheduler policies (`fBW` vs `fHoL`) "
            "across two offered load conditions (`sendInterval = 0.5ms` high load vs `sendInterval = 1.0ms` moderate load).\n\n"
            "#### [script] Performance and Protocol Dynamics Across Offered Loads\n\n"
            "- **High Load (`sendInterval = 0.5ms` / Offered Load ~4.8 Mbit/s aggregate):**\n"
            "  - **Continuous Queue Backlog & 100% DL HE-MU Transmission**: High packet arrival rates keep queues continuously backlogged across all 3 STAs. "
            "Under `fBW` policy (which allocates 2 x 106-tone RUs in 20 MHz bandwidth for 3 active STAs), the AP always finds >= 2 candidate STAs with queued data whenever it gains channel access. "
            "Consequently, both `TriggeredBar` (4183 HE-MU PPDUs vs 4 HE-SU) and `SequentialBar` (3715 HE-MU PPDUs vs 4 HE-SU) achieve virtually 100% DL HE-MU transmissions.\n"
            "  - **Overhead Comparison**: `TriggeredBar` achieves **4.787 Mbit/s** aggregate goodput by replacing individual unicast BAR/BA cycles with a single MU-BAR Trigger and parallel UL HE-TB Block Acks. "
            "`SequentialBar` incurs a 11.2% throughput penalty (**4.249 Mbit/s**) due to sequential BAR/BA frame exchange overhead (932 BARs and 932 BAs).\n"
            "  - **`fHoL` Policy (`TriggeredBarfHoL` & `SequentialBarfHoL`)**: `fHoL` allocates 4 x 52-tone RUs (`count >= 3` candidates selects 4 RUs), scheduling all 3 STAs into every DL HE-MU PPDU concurrently (3021 HE-MU PPDUs, 0 HE-SU).\n\n"
            "- **Moderate Load (`sendInterval = 1.0ms` / Offered Load ~2.4 Mbit/s aggregate):**\n"
            "  - **Intermittent Arrival & HE-SU Single-User Fallbacks**: Under moderate offered load (1 pkt/ms per host), AP queues drain completely between channel access attempts. "
            "In `TriggeredBar_1ms` (`fBW`), when the AP gains channel access, frequently only 1 station has a newly arrived packet in `pendingQueue`. "
            "Because `HeHcf::tryStartDlMuFrameSequence` requires >= 2 candidate STAs with active Block Ack agreements to build a multi-user frame, having only 1 candidate forces `HeHcf` to fall back to Single-User **HE-SU 20 MHz PPDU** (`Hcf::startFrameSequence`). "
            "This results in **703 HE-SU fallbacks** alongside 1400 HE-MU PPDUs in `TriggeredBar_1ms` (and similarly in `TriggeredBarfHoL_1ms`).\n"
            "  - **Sequential BAR Dynamics**: In `SequentialBar_1ms`, the longer duration of sequential unicast BAR/BA frame exchanges delays medium release, allowing packets to backlog across multiple STAs by the next AP contention win, maintaining 2094 HE-MU PPDUs and 0 HE-SU fallbacks.\n"
            "  - **Full Delivery**: All 1.0 ms configurations (except 52-tone sequential BAR) deliver 100% of offered load (**2.400 Mbit/s**) with sub-millisecond end-to-end delays (~0.44–0.52 ms).\n\n"
            f"**{he_mu_check['status']}: HE-MU payload decoding.** {he_mu_check['evidence']} decode as **QoS Data** with radiotap A-MPDU status."
        )
    elif subdir in DL_OFDMA_SUBDIRS:
        he_mu_check = next(check for check in checks if check["id"] == "dl-ofdma-he-mu-payload-decode")
        analysis_text = (
            "The scheduled downlink captures contain the expected **Trigger** frames and HE-TB **Block Ack** responses for the DL-MU acknowledgment exchange "
            "described by IEEE Std 802.11-2024 Clauses 26.5.1 and 26.5.2.3.3. The radiotap suffixes also distinguish HE-MU transmissions from HE-TB responses. "
            f"**{he_mu_check['status']}: HE-MU payload decoding.** {he_mu_check['evidence']} decode as **QoS Data** with radiotap A-MPDU status; "
            "none are misclassified as Association Request or Control Subtype 0.\n\n"
            "The packet tables verify the protocol-visible exchange structure and the corrected aggregate serialization boundary, but scheduler telemetry remains "
            "authoritative for per-user RU allocation. "
            "The corrected MPDUs expose unicast receiver addresses, so the asymmetric tables can group observations by STA. These address-scoped counts include "
            "protocol observations rather than delivered application packets; measure scheduler priorities and offered-load satisfaction from aligned per-user scheduler "
            "and application results. IEEE 802.11 does not prescribe INET's backlog- or head-of-line scheduling policies."
        )
    elif "bss_coloring" in subdir:
        bss_check = next(check for check in checks if check["id"] == "bss-coloring-observable-separation")
        interpretation = (
            "The differing distribution is only a screening signal; the separate five-seed result campaign validates direct OBSS classification, threshold, CCA, power-limit, and reuse-decision telemetry."
            if bss_check["status"] == "PASS" else
            "The identical result does not violate the standard, but this bounded scalar-medium workload provides no evidence that coloring or the threshold sweep changed protocol behavior."
        )
        analysis_text = (
            f"**{bss_check['status']}: BSS-coloring separation.** {bss_check['evidence']}. "
            "IEEE Std 802.11-2024 Clause 26.10 permits eligible inter-BSS reuse after OBSS/PD "
            "classification; it does not guarantee a throughput improvement, and a more permissive threshold can increase interference. "
            f"{interpretation} The current model reports the standards-defined threshold/power coupling but does not dynamically adapt OBSS/PD or apply that limit to later transmissions."
        )
    elif "dynamic_frag" in subdir or "dynamic_fragmentation" in subdir:
        analysis_text = (
            "IEEE Std 802.11-2024 Clause 26.3 gates dynamic fragmentation on negotiated peer capability; it does not require fragment size to adapt to channel conditions. "
            "In this implementation the dynamic and static policies use the same 500-byte sizing policy after the capability gate, so their detailed result-analysis "
            "traces are expected to overlap. This packet table contains only `DynamicFragmentation`; it cannot establish a higher fragment count without the static "
            "and unfragmented controls, nor can Block Ack subtype counts alone establish the fragment bitmap."
        )
    elif "er_su" in subdir or "he_er_su" in subdir:
        er_check = next(check for check in checks if check["id"] == "he-er-format-CellBoundaryHeErSu")
        analysis_text = (
            f"**{er_check['status']}: HE-ER-SU payload selection.** {er_check['evidence']}. "
            "IEEE Std 802.11-2024 Clause 27.3.7 restricts HE ER SU to a single 242-tone or 106-tone RU and MCS 0–2 "
            "(242-tone) or MCS 0 (106-tone); DCM is optional. The standard does not guarantee a range gain on every channel, "
            "but a configuration claiming HE-ER-SU payload coverage must first select that PPDU format. The matched five-seed 340 m sweep in this walkthrough "
            "uses equal MCS 0 data fields and reports application delivery together with incorrect-reception observations, isolating the modeled HE-SIG-A repetition gain."
        )
    elif "rate_adaptation" in subdir or "he_rate_adaptation" in subdir:
        analysis_text = (
            "IEEE 802.11 constrains negotiated HE modes but does not mandate a Minstrel algorithm. These packet counts therefore cannot establish adaptation, "
            "and a control/data ratio is not reliable evidence of retransmission or probing. Use the aligned selected-MCS/NSS, EWMA probability, transmission-outcome, "
            "and retry vectors documented above. INET's HE Minstrel remains a simplified implementation without scheduler-context or localized-fading adaptation."
        )
    elif "channel_widths" in subdir or "he_channel_widths" in subdir:
        analysis_text = (
            "IEEE Std 802.11-2024 Table 27-1 defines 20, 40, 80, and 160 MHz HE channel-width encodings, but the standard does not require packet count or throughput "
            "to scale linearly with width. The run-0 frame totals here are non-monotonic because aggregation, RU scheduling, and fixed overhead change the number of "
            "transmitted frames. The five-run sink goodput and delay analysis above is the appropriate capacity comparison; the radiotap bandwidth suffix is not."
        )
    elif "ndp_feedback" in subdir:
        analysis_text = (
            "The capture contains both **Trigger** frames and zero-length HE TB feedback NDP observations, consistent with the NDP Feedback Report Poll exchange "
            "defined by IEEE Std 802.11-2024 Clause 26.5.7 and Annex G.5. One NFRP Trigger can allocate multiple feedback resources, so the counts need not be one-to-one. "
            "The generic Control-subtype label does not by itself prove Trigger Type 7; verify the Trigger field or simulator NFRP telemetry when conformance detail matters."
        )
    elif "ul_multitid" in subdir or "multi_tid_block_ack" in subdir:
        multi_tid_check = next(
            check for check in checks if check["id"] == "multi-tid-ba-fields"
        )
        analysis_text = (
            f"**{multi_tid_check['status']}: decoded Multi-STA Block Ack fields.** "
            f"{multi_tid_check['evidence']}. The table above is direct TShark decoding "
            "of BA Control Type 11 and its per-AID/TID entries, as specified by IEEE Std "
            "802.11-2024 Clause 9.3.1.8.6. It establishes the acknowledged recipient/TID "
            "identities in the captured frames, not payload delivery or end-to-end reliability."
        )
    elif "opmode_indication" in subdir or "operating_mode_indication" in subdir:
        analysis_text = (
            "The data and acknowledgment counts show traffic before and after the configured operating-mode change, but frame subtype statistics cannot expose the "
            "Operating Mode Indication element or OM Control subfield. Standard behavior must be checked from those fields and the receiver's applied channel-width/NSS state, "
            "not inferred from the packet total."
        )
    elif "dl_mu_mimo" in subdir or "ul_mu_mimo" in subdir or "mu_mimo" in subdir:
        analysis_text = (
            "Decoded Radiotap HE-MU (bit 24) headers and spatial stream starting indices (`heStreamStartIndex`) in captured frames directly prove non-overlapping spatial stream allocations across multiplexed users in DL MU-MIMO PPDUs."
        )
    elif "bsr" in subdir or "he_bsr" in subdir:
        analysis_text = (
            "The scheduled conditions contain the expected Trigger/response activity, but a BSR is an A-Control scheduling input rather than a frame subtype. "
            "IEEE Std 802.11-2024 Clause 26.5.5 requires the report contents and capability conditions; use the AP-reported and scheduled-backlog telemetry documented above. "
            "QoS Data counts are not evidence that a BSR was fresh or that the reported bytes were delivered."
        )
    elif "preamble_puncturing" in subdir or "packet_extension" in subdir or "bcc_ldpc" in subdir or "he_features" in subdir:
        analysis_text = (
            "`BccBaseline` and `PreamblePuncturing` have identical frame counts in this run. That is not a standards violation and does not mean the PHY configuration was identical: "
            "preamble puncturing changes the usable subchannels/RU placement, while a fully served offered load can leave packet totals unchanged. Validate the mask and puncture-aware "
            "RU allocation with the vectors documented above; packet totals alone cannot prove them."
        )
    elif "frequency_selective_channel" in subdir:
        analysis_text = (
            "The different frame mixtures show that the two configured access paths executed different exchanges, but aggregate subtype counts cannot validate a frequency-selective "
            "channel or per-RU isolation. IEEE HE DL-MU behavior must be established with RU allocation and per-RU reception/SNIR outcomes together with sink delivery; the channel model "
            "is an implementation choice rather than an IEEE-mandated propagation model."
        )
    else:
        analysis_text = (
            "Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, "
            "while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. "
            "Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization "
            "and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency "
            "of the chosen configurations."
        )

    md.append(analysis_text + "\n")
    return "".join(md)


def find_level_two_section(content, label):
    headings = list(re.finditer(r"^##\s+(.+?)\s*$", content, re.MULTILINE))
    index = next(
        (
            position for position, match in enumerate(headings)
            if normalize_heading_label(match.group(1)) == label.lower()
        ),
        None,
    )
    return headings, index


def replace_generated_section(content, md_content):
    generated_content = f"{GENERATED_BEGIN}\n{md_content.rstrip()}\n{GENERATED_END}\n"
    if GENERATED_BEGIN in content or GENERATED_END in content:
        if content.count(GENERATED_BEGIN) != 1 or content.count(GENERATED_END) != 1:
            raise ValueError("Malformed generated-section markers")
        begin = content.index(GENERATED_BEGIN)
        end = content.index(GENERATED_END, begin) + len(GENERATED_END)
        _, canonical_index = find_level_two_section(content, "PCAP statistics")
        if canonical_index is None:
            return content[:begin] + generated_content.rstrip() + content[end:]
        # Move old tail-level generated blocks into the canonical section.
        without_generated = (
            content[:begin].rstrip() + "\n\n" + content[end:].lstrip("\n")
        )
        content = without_generated

    legacy_header = "## 802.11 Packet Type Statistics"
    if legacy_header in content:
        # One-time migration of the legacy generated tail.
        begin = content.index(legacy_header)
        content = content[:begin].rstrip() + "\n"

    headings, canonical_index = find_level_two_section(
        content, "PCAP statistics"
    )
    if canonical_index is None:
        return content.rstrip() + "\n\n" + generated_content
    canonical = headings[canonical_index]
    section_end = (
        headings[canonical_index + 1].start()
        if canonical_index + 1 < len(headings)
        else len(content)
    )
    prefix = content[:section_end].rstrip()
    suffix = content[section_end:].lstrip("\n")
    return f"{prefix}\n\n{generated_content}\n{suffix}".rstrip() + "\n"


def update_walkthrough(content, md_content, session_id):
    updated = replace_generated_section(content, md_content)
    return update_script_results_session(updated, "PCAP", session_id)


def update_walkthrough_file(subdir, md_content, session_id):
    walkthrough_path = EXAMPLE_ROOT / subdir / "walkthrough.md"
    if not walkthrough_path.exists():
        print(f"Walkthrough file not found: {walkthrough_path}")
        return

    with open(walkthrough_path, "r") as f:
        content = f.read()
    try:
        new_content = update_walkthrough(content, md_content, session_id)
    except ValueError as error:
        raise RuntimeError(f"{error} in {walkthrough_path}") from error

    atomic_write_text(walkthrough_path, new_content)
    print(f"Updated: {walkthrough_path.relative_to(REPOSITORY_ROOT)}")

def build_argument_parser(subdir_choices=None):
    parser = argparse.ArgumentParser(
        description="Analyze PCAPs recorded by a shared IEEE 802.11 campaign"
    )
    parser.add_argument(
        "--suite",
        type=Path,
        required=True,
        help="suite descriptor under examples/ieee80211/analysis/suites",
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--index",
        action="store_true",
        help="index PCAPs already recorded by the shared multi-run campaign",
    )
    mode.add_argument("--reuse", action="store_true", help="reuse only inputs validated by the existing manifest")
    parser.add_argument("--subdir", action="append", choices=subdir_choices,
                        help="analyze one group; repeat for multiple groups (default: all)")
    parser.add_argument(
        "--config",
        action="append",
        help="analyze only the named configuration; repeat for multiple configurations",
    )
    parser.add_argument("--run", type=int, default=0, help="OMNeT++ run number (default: 0)")
    parser.add_argument(
        "--session-id",
        help="index or reuse this immutable YYYYMMDDTHHMMSSZ capture session",
    )
    parser.add_argument(
        "--allow-failed-evidence",
        action="store_true",
        help="write reports but return success even when an evidence check is FAIL",
    )
    parser.add_argument(
        "--capture-only",
        action="store_true",
        help="index captures and their immutable manifest, then stop",
    )
    parser.add_argument(
        "--update-walkthrough",
        action="store_true",
        help="explicitly update marker-bounded walkthrough sections",
    )
    return parser


def parse_args(argv=None):
    arguments = sys.argv[1:] if argv is None else list(argv)
    suite_parser = argparse.ArgumentParser(add_help=False)
    suite_parser.add_argument("--suite", type=Path)
    selected_suite, _ = suite_parser.parse_known_args(arguments)
    if selected_suite.suite is None:
        return build_argument_parser().parse_args(arguments)
    suite = load_suite(selected_suite.suite, REPOSITORY_ROOT)
    configure_suite(
        suite,
        ANALYSIS_ROOT / "generated" / suite.suite,
    )
    return build_argument_parser(
        sorted(subdirs_configs)
    ).parse_args(arguments)


def main():
    global CONFIG_ORDER
    args = parse_args()
    if args.capture_only and not args.index:
        raise ValueError("--capture-only requires --index")
    selected_subdirs = args.subdir or list(subdirs_configs)
    if args.config:
        selected_configs = set(args.config)
        available_configs = {
            config
            for subdir in selected_subdirs
            for config in subdirs_configs[subdir]
        }
        unknown_configs = sorted(selected_configs - available_configs)
        if unknown_configs:
            raise ValueError(
                "Unknown configuration(s) for the selected scenarios: "
                + ", ".join(unknown_configs)
            )
        for subdir in selected_subdirs:
            subdirs_configs[subdir] = [
                config
                for config in subdirs_configs[subdir]
                if config in selected_configs
            ]
        selected_subdirs = [
            subdir for subdir in selected_subdirs if subdirs_configs[subdir]
        ]
    if args.index:
        manifest = build_capture_manifest(
            selected_subdirs,
            args.run,
            args.session_id,
        )
    else:
        manifest = load_and_validate_manifest(
            selected_subdirs, args.run, args.session_id
        )
    if args.capture_only:
        print(
            f"CAPTURED session {manifest['session_id']}; "
            "analysis and walkthrough publication were not run"
        )
        return
    selected_manifest = (
        _history_path(manifest["session_id"])
        if args.index or args.session_id is not None
        else MANIFEST_PATH
    )
    manifest["_selected_manifest"] = manifest_provenance(selected_manifest)
    capture_map = capture_map_from_manifest(manifest, selected_subdirs, args.run)

    all_results = {}
    all_checks = {}
    for subdir in selected_subdirs:
        considered = subdirs_configs[subdir]
        res = analyze_subdirectory(subdir, considered, capture_map[subdir])
        if res:
            all_results[subdir] = res
            all_checks[subdir] = validate_evidence_checks(
                evaluate_evidence(res, subdir), subdir
            )

    # Gather the union of all packet types across all results to use consistent colors
    global_packet_types = set()
    for subdir, subdir_res in all_results.items():
        for config_name, res in subdir_res.items():
            if "global" in res:
                stats = res["global"]["stats"]
                for key in stats.keys():
                    name = unpack_key_to_name(key)
                    global_packet_types.add(name)

    global_packet_types = sorted(list(global_packet_types))

    # Assign a permanent color to each global packet type using HSL-based mapping
    global_color_map = {pt: get_packet_color(pt) for pt in global_packet_types}

    for subdir, res in all_results.items():
        CONFIG_ORDER = {
            config: index
            for index, config in enumerate(subdirs_configs[subdir])
        }
        output_dir = campaign_result_directory(
            subdir,
            subdirs_configs[subdir][0],
            manifest["session_id"],
        ).parent

        res_1ms = {k: v for k, v in res.items() if k.endswith("_1ms")}
        res_0_5ms = {k: v for k, v in res.items() if not k.endswith("_1ms")}

        if res_1ms and res_0_5ms:
            plot_path_0_5ms = generate_stacked_bar_plot(
                res_0_5ms, subdir, global_color_map, output_dir, filename="packet_statistics.png"
            )
            write_packet_plot_provenance(plot_path_0_5ms, res_0_5ms, subdir, manifest)

            plot_path_1ms = generate_stacked_bar_plot(
                res_1ms, subdir, global_color_map, output_dir, filename="packet_statistics_1ms.png"
            )
            write_packet_plot_provenance(plot_path_1ms, res_1ms, subdir, manifest)

            md_table = generate_markdown_tables(
                res, subdir, all_checks[subdir], manifest, plot_path_0_5ms, generated_plot_path_1ms=plot_path_1ms
            )
        else:
            plot_path = generate_stacked_bar_plot(
                res, subdir, global_color_map, output_dir
            )
            if plot_path is None:
                raise RuntimeError(f"{subdir}: no packet statistics plot generated")
            write_packet_plot_provenance(plot_path, res, subdir, manifest)
            md_table = generate_markdown_tables(
                res, subdir, all_checks[subdir], manifest, plot_path
            )
        if md_table and args.update_walkthrough:
            update_walkthrough_file(
                subdir,
                md_table,
                manifest["session_id"],
            )

    summary_json_path = ANALYSIS_OUTPUT_DIR / "packet_metrics.json"
    serialized = {
        "_provenance": {
            "schema_version": 1,
            "capture_session_id": manifest["session_id"],
            "repository_revision": manifest["repository_revision"],
            "suite": manifest["suite"],
            "suite_descriptor": manifest["suite_descriptor"],
            "capture_source_diff_sha256": manifest["capture_source_diff_sha256"],
            "analysis_script_sha256": manifest["analysis_script_sha256"],
            "tshark_version": manifest["tshark_version"],
            "capinfos_version": manifest.get("capinfos_version"),
            "capture_manifest": manifest["_selected_manifest"],
            "run_number": args.run,
            "entries": [
                {
                    "subdir": entry["subdir"],
                    "config": entry["config"],
                    "run_number": entry["run_number"],
                    "seed_set": entry["seed_set"],
                    "captures": entry["captures"],
                }
                for entry in manifest["entries"]
                if entry["subdir"] in selected_subdirs
                and entry["run_number"] == args.run
            ],
            "cross_layer_session_alignment": (
                "ASSESSED: every capture is bound to the scalar metadata from "
                "the same result directory, configuration, run, seed, and "
                "simulation trajectory"
            ),
        }
    }
    for subdir, subdir_res in all_results.items():
        serialized[subdir] = {}
        serialized[subdir]["_evidence_checks"] = all_checks[subdir]
        for config_name, res in subdir_res.items():
            serialized[subdir][config_name] = {}
            if "global" in res:
                g = res["global"]
                serialized[subdir][config_name]["global"] = {
                    "total": g["total"],
                    "used_ap_only": g["used_ap_only"],
                    "captures": g["captures"],
                    "display_filter": g["display_filter"],
                    "stats": {",".join(str(x) for x in k): v for k, v in g["stats"].items()},
                    "timeline": g["timeline"],
                    "he_trigger_allocations": g["he_trigger_allocations"],
                    "multi_sta_block_ack_records": g["multi_sta_block_ack_records"],
                    "compressed_block_ack_records": g["compressed_block_ack_records"],
                }
            if "per_flow" in res:
                serialized[subdir][config_name]["per_flow"] = {}
                for h_name, h_val in res["per_flow"].items():
                    serialized[subdir][config_name]["per_flow"][h_name] = {
                        "total": h_val["total"],
                        "stats": {",".join(str(x) for x in k): v for k, v in h_val["stats"].items()}
                    }
            if "mpdu_observations" in res:
                serialized[subdir][config_name]["mpdu_observations"] = res["mpdu_observations"]
    atomic_write_text(
        summary_json_path,
        json.dumps(serialized, indent=2, sort_keys=True, allow_nan=False) + "\n",
    )
    print(f"Finished analysis. Output written to: {summary_json_path.relative_to(REPOSITORY_ROOT)}")
    failed = [
        f"{subdir}/{check['id']}"
        for subdir, checks in all_checks.items()
        for check in checks
        if check["status"] == "FAIL"
    ]
    if failed and not args.allow_failed_evidence:
        print("Failed evidence checks: " + ", ".join(failed), file=sys.stderr)
        raise SystemExit(2)

if __name__ == "__main__":
    main()
