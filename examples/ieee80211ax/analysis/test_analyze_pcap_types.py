import tempfile
import unittest
import json
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from analyze_pcap_types import (
    GENERATED_BEGIN,
    GENERATED_END,
    MANIFEST_PATH,
    SUPPORTED_MANIFEST_SCHEMAS,
    capture_map_from_manifest,
    capture_source_state,
    calculate_phy_rate,
    compact_statistics_markdown,
    decode_eht_fields,
    decode_he_fields,
    decode_legacy_fields,
    estimate_airtime,
    evaluate_evidence,
    extract_frame_timeline,
    get_config_pcap_stats,
    replace_generated_section,
    load_json_without_duplicates,
    manifest_provenance,
    manifest_schema_version,
    parse_capinfos_interfaces,
    parse_capinfos_table,
    publish_capture_manifest,
    selected_manifest_path,
    select_representative_timeline,
    timeline_filter_for_subdir,
    timeline_markdown,
    validate_capture_decode,
    validate_capture_metadata,
    validate_entry_binding,
    validate_evidence_checks,
)


class DecodeHeFieldsTest(unittest.TestCase):

    def test_decodes_tshark_he_su_fields(self):
        self.assertEqual(
            decode_he_fields("0x0000", "0x0002", "0x0001", "0x0001", "0x0001", "0x0002"),
            ("HE-SU", "HE-MCS 2", "40 MHz", "1.6 us", "2", "LDPC"),
        )

    def test_decodes_extended_range_and_ru(self):
        self.assertEqual(
            decode_he_fields("0x0001", "0x0000", "0x0000", "0x0007", "0x0002", "0x0001"),
            ("HE-ER-SU", "HE-MCS 0", "242-tone RU", "3.2 us", "1", "BCC"),
        )

    def test_preserves_unknown_values_as_unknown(self):
        self.assertEqual(
            decode_he_fields("", "", "", "", "", ""),
            ("HE", "HE", "", "", "", ""),
        )

    def test_he_su_airtime_uses_modeled_preamble(self):
        airtime = estimate_airtime(
            "1", "13", 14, "Config", "subdir", standard="HE-SU",
            data_rate=7.3125e6)
        self.assertAlmostEqual(airtime, 36e-6 + 14 * 8 / 7.3125e6)

    def test_he_er_su_airtime_uses_repeated_sig_a(self):
        airtime = estimate_airtime(
            "2", "8", 300, "Config", "subdir", standard="HE-ER-SU",
            data_rate=7.3125e6)
        self.assertAlmostEqual(airtime, 44e-6 + 300 * 8 / 7.3125e6)

    def test_ampdu_followup_observation_does_not_repeat_preamble(self):
        airtime = estimate_airtime(
            "2", "8", 300, "Config", "subdir", standard="HE-SU",
            data_rate=7.3125e6, include_preamble=False)
        self.assertAlmostEqual(airtime, 300 * 8 / 7.3125e6)


class DecodeEhtFieldsTest(unittest.TestCase):

    def test_decodes_only_authoritative_eht_su_fields(self):
        self.assertEqual(
            decode_eht_fields(
                "0x00080004", "0x00018003", "1", "0x01d00096",
                "13", "0", "1",
            ),
            ("EHT", "EHT-MCS 13", "160 MHz", "1.6 us", "2", "BCC"),
        )

    def test_eht_known_zero_does_not_fabricate_defaults(self):
        self.assertEqual(
            decode_eht_fields("0", "0", "0", "0", "13", "0", "1"),
            ("EHT", "EHT", "", "", "", ""),
        )
        self.assertIsNone(calculate_phy_rate("EHT", "EHT", "", "", ""))

    def test_eht_rate_supports_4096_qam(self):
        self.assertAlmostEqual(
            calculate_phy_rate(
                "EHT", "EHT-MCS 13", "160 MHz", "1.6 us", "2"
            ),
            1960 * 2 * 10 / 14.4e-6,
        )


class DecodeLegacyFieldsTest(unittest.TestCase):

    def test_legacy_rate_does_not_fabricate_phy_parameters(self):
        self.assertEqual(
            decode_legacy_fields("24"),
            ("Legacy", "24 Mbps", "", "", "", ""),
        )

    def test_statistics_entry_point_keeps_legacy_phy_parameters_unknown(self):
        fields = [""] * 44
        fields[0] = "0"
        fields[1] = "2"
        fields[2] = "0"
        fields[3] = "100"
        fields[4] = "20"
        fields[8] = "24"
        fields[43] = "1"
        result = SimpleNamespace(
            returncode=0,
            stdout="\t".join(fields) + "\n",
            stderr="",
        )
        with patch("analyze_pcap_types.get_sim_time_limit", return_value=1):
            with patch("analyze_pcap_types.validate_capture_decode"):
                with patch(
                    "analyze_pcap_types.subprocess.run", return_value=result
                ):
                    statistics, total = get_config_pcap_stats(
                        ["capture.pcapng"], "Config", "legacy"
                    )
        self.assertEqual(total, 1)
        key = next(iter(statistics))
        self.assertEqual(
            key[2:8],
            ("Legacy", "24 Mbps", "", "", "", ""),
        )


class GeneratedSectionTest(unittest.TestCase):

    def test_marker_bounded_update_preserves_manual_tail(self):
        original = f"Manual prefix\n\n{GENERATED_BEGIN}\nold\n{GENERATED_END}\n\nManual tail\n"
        updated = replace_generated_section(original, "new")
        self.assertEqual(
            updated,
            f"Manual prefix\n\n{GENERATED_BEGIN}\nnew\n{GENERATED_END}\n\nManual tail\n",
        )

    def test_generated_bundle_is_inserted_inside_pcap_section(self):
        original = (
            "# Walkthrough\n\n## **PCAP statistics**\n\nAuthored summary.\n\n"
            "## Frame exchange analysis\n\nTimeline.\n"
        )
        updated = replace_generated_section(original, "generated")
        self.assertLess(
            updated.index("generated"),
            updated.index("## Frame exchange analysis"),
        )
        self.assertIn("Authored summary.", updated)

    def test_tail_level_bundle_is_migrated_inside_pcap_section(self):
        original = (
            "# Walkthrough\n\n## PCAP statistics\n\nAuthored summary.\n\n"
            "## Frame exchange analysis\n\nTimeline.\n\n"
            f"{GENERATED_BEGIN}\nold\n{GENERATED_END}\n"
        )
        updated = replace_generated_section(original, "new")
        self.assertLess(
            updated.index(GENERATED_BEGIN),
            updated.index("## Frame exchange analysis"),
        )
        self.assertNotIn("\nold\n", updated)
        self.assertIn("\nnew\n", updated)

    def test_migrates_legacy_generated_tail(self):
        original = "Manual prefix\n\n## 802.11 Packet Type Statistics\nlegacy\n"
        updated = replace_generated_section(original, "generated")
        self.assertEqual(
            updated,
            f"Manual prefix\n\n{GENERATED_BEGIN}\ngenerated\n{GENERATED_END}\n",
        )

    def test_rejects_unbalanced_markers(self):
        with self.assertRaises(ValueError):
            replace_generated_section(f"prefix\n{GENERATED_BEGIN}\n", "new")


class CaptureValidationTest(unittest.TestCase):

    def test_empty_capture_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory) / "empty.pcapng"
            capture.touch()
            with self.assertRaisesRegex(RuntimeError, "missing or empty"):
                validate_capture_decode(capture)

    def test_tshark_decode_failure_is_an_error(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory) / "capture.pcapng"
            capture.write_bytes(b"not-empty")
            result = SimpleNamespace(returncode=2, stdout="", stderr="bad capture")
            with patch("analyze_pcap_types.subprocess.run", return_value=result):
                with self.assertRaisesRegex(RuntimeError, "exit 2: bad capture"):
                    validate_capture_decode(capture)

    def test_first_frame_requires_all_decisive_decode_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory) / "capture.pcapng"
            capture.write_bytes(b"capture")
            result = SimpleNamespace(returncode=0, stdout="1\t0\t\n", stderr="")
            with patch("analyze_pcap_types.subprocess.run", return_value=result):
                with self.assertRaisesRegex(RuntimeError, "First frame lacks"):
                    validate_capture_decode(capture)

    def test_feature_timeline_filters_select_decisive_frames(self):
        self.assertIn("wlan.fc.subtype == 10", timeline_filter_for_subdir("twt"))
        self.assertIn(
            "wlan.fc.subtype == 2", timeline_filter_for_subdir("dl_ofdma")
        )

    def test_twt_timeline_keeps_ps_poll_before_responder_data(self):
        def row(number, time, name):
            return {
                "capture": f"capture-{number % 2}.pcapng",
                "frame_number": number,
                "simulation_time_s": time,
                "transmitter": "00:01",
                "receiver": "00:02",
                "frame_type": "1",
                "frame_subtype": "10" if name == "Control: PS-Poll" else "13",
                "frame_name": name,
                "retry": False,
                "sequence_number": number,
                "fragment_number": 0,
                "more_fragments": False,
            }

        rows = [
            *(row(index, index / 10, "Control: Ack") for index in range(8)),
            row(8, 0.8, "Control: PS-Poll"),
            row(9, 0.9, "Data: QoS Data"),
        ]
        selected = select_representative_timeline(rows, "twt", limit=4)
        names = [item["frame_name"] for item in selected]
        self.assertIn("Control: PS-Poll", names)
        self.assertIn("Data: QoS Data", names)
        self.assertLess(
            names.index("Control: PS-Poll"), names.index("Data: QoS Data")
        )

    def test_timeline_reports_capture_local_frame_and_he_fields(self):
        markdown = timeline_markdown([{
            "capture": "results/ap.wlan0.pcapng",
            "frame_number": 7,
            "simulation_time_s": 1.25,
            "transmitter": "00:00:00:00:00:01",
            "receiver": "00:00:00:00:00:02",
            "direction": "from DS",
            "frame_name": "Data: QoS Data",
            "retry": True,
            "sequence_number": 42,
            "fragment_number": 0,
            "more_fragments": False,
            "tid": 5,
            "ampdu": True,
            "ampdu_reference": "9",
            "phy": {
                "format": "HE-MU",
                "mcs": "HE-MCS 3",
                "bandwidth_or_ru": "52-tone RU",
                "guard_interval": "0.8 us",
                "nss": "1",
                "coding": "LDPC",
            },
        }])
        self.assertIn("`ap.wlan0.pcapng:7`", markdown)
        self.assertIn(
            "direction=from DS, retry=1, seq=42, frag=0, more-frag=0, "
            "TID=5, A-MPDU=9",
            markdown,
        )
        self.assertIn(
            "HE-MU, HE-MCS 3, 52-tone RU, NSS 1, GI 0.8 us, LDPC",
            markdown,
        )

    def test_timeline_parser_orders_frames_and_keeps_direction_fields(self):
        with tempfile.TemporaryDirectory(
            dir=Path(__file__).resolve().parent
        ) as directory:
            capture = Path(directory) / "ap.wlan0.pcapng"
            capture.write_bytes(b"capture")
            validation = SimpleNamespace(
                returncode=0, stdout="1\t0\t2\n", stderr=""
            )
            rows = [
                "8\t2.000\t00:01\t00:02\t\t\t2\t8\t1\t9\t1\t5\t1\t7\t1\t2\t3\t1\t5\t0\t1\t1\t0\t1",
                "7\t1.000\t00:01\t00:02\t\t\t1\t2\t0\t8\t0\t\t0\t\t1\t3\t1\t0\t4\t2\t1\t0\t0\t0",
            ]
            decode = SimpleNamespace(
                returncode=0, stdout="\n".join(rows) + "\n", stderr=""
            )
            with patch(
                "analyze_pcap_types.subprocess.run",
                side_effect=[validation, decode],
            ):
                timeline = extract_frame_timeline(
                    [capture], "dl_ofdma", limit=2
                )
        self.assertEqual(
            [frame["frame_number"] for frame in timeline], [7, 8]
        )
        self.assertEqual(timeline[1]["direction"], "to DS")
        self.assertTrue(timeline[1]["more_fragments"])
        self.assertEqual(timeline[1]["phy"]["format"], "HE-MU")

    def test_unknown_evidence_status_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "invalid status"):
            validate_evidence_checks([{
                "id": "check",
                "status": "SKIPPED",
                "requirement": "observable",
                "evidence": "none",
            }], "sample")

    def test_compact_statistics_table_states_counting_limits(self):
        key = (
            "2", "8", "HE-SU", "HE-MCS 2", "20 MHz", "0.8 us",
            "1", "LDPC", False, False,
        )
        markdown = compact_statistics_markdown({
            "Treatment": {
                "global": {
                    "total": 4,
                    "used_ap_only": True,
                    "captures": [
                        "examples/ieee80211ax/sample/results/ap.wlan0.pcapng"
                    ],
                    "display_filter": "wlan.fc.type == 2",
                    "stats": {
                        key: {
                            "count": 4,
                            "airtime_evidence_status": "AVAILABLE",
                            "airtime_pct": 100.0,
                            "airtime_sim_pct": 12.5,
                        }
                    },
                }
            }
        })
        self.assertIn(
            "AP interface(s); capture observations<br>"
            "`examples/ieee80211ax/sample/results/ap.wlan0.pcapng`",
            markdown,
        )
        self.assertIn("| `wlan.fc.type == 2` | 4 |", markdown)
        self.assertIn("Data: QoS Data [HE-SU", markdown)
        self.assertIn("Not delivery or de-duplicated transmissions", markdown)


class DlOfdmaEvidenceTest(unittest.TestCase):

    def test_he_mu_qos_ampdu_and_per_flow_attribution_pass(self):
        key = ("2", "8", "HE-MU", "HE-MCS 1", "52-tone RU",
               "3.2 us", "1", "LDPC", True, False)
        config_results = {}
        for config_name in ("BacklogBased", "HoLMinDelay", "BacklogBased4ms", "HoLMinDelay4ms", "BacklogBased3ms", "HoLMinDelay3ms", "BacklogBased3_5ms", "HoLMinDelay3_5ms", "BacklogBased2_5ms", "HoLMinDelay2_5ms", "BacklogBased1_5ms", "HoLMinDelay1_5ms"):
            per_flow = {
                host_name: {"stats": {key: {"count": 1}}, "total": 1}
                for host_name in ("host[0]", "host[1]", "host[2]")
            }
            config_results[config_name] = {
                "global": {"stats": {key: {"count": 3}}, "total": 3},
                "per_flow": per_flow,
            }

        checks = {
            check["id"]: check
            for check in evaluate_evidence(config_results, "dl_ofdma")
        }

        self.assertEqual(checks["dl-ofdma-he-mu-payload-decode"]["status"], "PASS")
        self.assertEqual(checks["dl-ofdma-per-user-attribution"]["status"], "PASS")


class CaptureManifestTest(unittest.TestCase):

    CAPINFOS = (
        "File name,File type,File encapsulation,File time precision,Packet size limit,"
        "Number of packets,File size (bytes),Data size (bytes),Capture duration (seconds),"
        "Start time,End time,Strict time order\n"
        "capture.pcapng,pcapng,ieee-802-11-radiotap,microseconds,(not set),"
        "3,128,64,0.5,0.25,0.75,True\n"
    )

    def test_capinfos_parser_and_validation(self):
        metadata = parse_capinfos_table(self.CAPINFOS)
        metadata["interfaces"] = parse_capinfos_interfaces(
            "Interface #0 info:\n"
            " Name = wlan0\n"
            " Description = ap.wlan0\n"
            " Capture length = 0\n"
            " Time precision = microseconds (6)\n"
        )
        validate_capture_metadata(metadata)
        self.assertEqual(metadata["packet_count"], 3)
        self.assertEqual(metadata["interfaces"][0]["description"], "ap.wlan0")
        metadata["strict_time_order"] = False
        with self.assertRaisesRegex(RuntimeError, "strict timestamp order"):
            validate_capture_metadata(metadata)

    def test_capture_metadata_rejects_inconsistent_duration_and_missing_interface(self):
        metadata = parse_capinfos_table(self.CAPINFOS)
        metadata["interfaces"] = [{
            "interface_id": 0,
            "time_precision": "microseconds (6)",
        }]
        metadata["duration_seconds"] = 0.25
        with self.assertRaisesRegex(RuntimeError, "duration is inconsistent"):
            validate_capture_metadata(metadata)
        metadata["duration_seconds"] = 0.5
        metadata["interfaces"] = []
        with self.assertRaisesRegex(RuntimeError, "interface metadata is missing"):
            validate_capture_metadata(metadata)

    def test_history_is_published_before_latest_and_never_clobbered(self):
        manifest = {"schema_version": 2, "session_id": "20260725T120000Z"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            latest = root / "latest.json"
            history = root / "history"
            with (
                patch("analyze_pcap_types.MANIFEST_PATH", latest),
                patch("analyze_pcap_types.MANIFEST_HISTORY_DIR", history),
            ):
                published = publish_capture_manifest(manifest)
                self.assertEqual(json.loads(published.read_text()), manifest)
                self.assertEqual(latest.read_text(), published.read_text())
                with self.assertRaises(FileExistsError):
                    publish_capture_manifest(manifest)
                self.assertEqual(json.loads(latest.read_text()), manifest)

    def test_source_digest_includes_head_diff_and_untracked_content(self):
        with tempfile.TemporaryDirectory(
            dir=Path(__file__).resolve().parent
        ) as directory:
            source = Path(directory) / "input.ned"
            source.write_text("first")
            relative = source.relative_to(
                Path(__file__).resolve().parents[3]
            ).as_posix()

            def digest():
                with (
                    patch(
                        "analyze_pcap_types.command_output",
                        side_effect=["revision", relative],
                    ),
                    patch(
                        "analyze_pcap_types.subprocess.run",
                        return_value=SimpleNamespace(stdout=b"tracked diff"),
                    ) as run,
                ):
                    result = capture_source_state()
                self.assertIn("HEAD", run.call_args.args[0])
                return result

            first = digest()
            source.write_text("second")
            second = digest()
            self.assertEqual(first[0], "revision")
            self.assertNotEqual(first[1], second[1])

    def test_session_selection_duplicate_keys_and_legacy_schema(self):
        self.assertEqual(SUPPORTED_MANIFEST_SCHEMAS, {1, 2})
        self.assertEqual(manifest_schema_version({"schema_version": 1}), 1)
        self.assertEqual(manifest_schema_version({"schema_version": 2}), 2)
        with self.assertRaisesRegex(RuntimeError, "Unsupported"):
            manifest_schema_version({"schema_version": 3})
        self.assertEqual(selected_manifest_path(), MANIFEST_PATH)
        self.assertTrue(
            str(selected_manifest_path("20260725T120000Z")).endswith(
                "pcapmanifests/20260725T120000Z.json"
            )
        )
        with tempfile.TemporaryDirectory(
            dir=Path(__file__).resolve().parent
        ) as directory:
            duplicate = Path(directory) / "duplicate.json"
            duplicate.write_text('{"schema_version": 1, "schema_version": 2}')
            with self.assertRaisesRegex(RuntimeError, "Duplicate JSON key"):
                load_json_without_duplicates(duplicate)

    def test_manifest_provenance_has_relative_path_and_hash(self):
        with tempfile.TemporaryDirectory(
            dir=Path(__file__).resolve().parent
        ) as directory:
            path = Path(directory) / "manifest.json"
            path.write_text("{}\n")
            provenance = manifest_provenance(path)
            self.assertFalse(Path(provenance["path"]).is_absolute())
            self.assertEqual(len(provenance["sha256"]), 64)

    def test_capture_map_preserves_requested_session_coverage(self):
        manifest = {
            "entries": [{
                "subdir": "dl_ofdma",
                "config": "EqualSizedRUs_fBW",
                "run_number": 0,
                "captures": [{"path": "examples/capture.pcapng"}],
            }]
        }
        mapping = capture_map_from_manifest(manifest, ["dl_ofdma"], 0)
        self.assertEqual(
            mapping["dl_ofdma"]["EqualSizedRUs_fBW"][0].name,
            "capture.pcapng",
        )

    def test_schema2_entry_binds_artifacts_to_session_config_run_and_seed(self):
        base = (
            "examples/ieee80211ax/dl_ofdma/results/packet-statistics/"
            "20260725T120000Z/EqualSizedRUs_fBW"
        )
        entry = {
            "subdir": "dl_ofdma",
            "config": "EqualSizedRUs_fBW",
            "run_number": 0,
            "seed_set": 7,
            "result_directory": base,
            "scalar": {
                "path": f"{base}/result.sca",
                "configname": "EqualSizedRUs_fBW",
                "runnumber": 0,
                "seedset": 7,
            },
            "captures": [{"path": f"{base}/ap.wlan0.pcap"}],
        }
        self.assertEqual(
            validate_entry_binding(entry, "20260725T120000Z"), []
        )
        entry["captures"][0]["path"] = (
            "examples/ieee80211ax/dl_ofdma/results/packet-statistics/"
            "20260724T120000Z/EqualSizedRUs_fBW/ap.wlan0.pcap"
        )
        entry["seed_set"] = 8
        errors = validate_entry_binding(entry, "20260725T120000Z")
        self.assertTrue(any("capture path" in error for error in errors))
        self.assertTrue(any("seedset" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
