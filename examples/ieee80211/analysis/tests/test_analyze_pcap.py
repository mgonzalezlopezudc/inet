import tempfile
import unittest
import json
import sys
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

ANALYSIS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ANALYSIS_ROOT))

import analyze_pcap


AX_SUITE = ANALYSIS_ROOT / "suites" / "ax.json"
BE_SUITE = ANALYSIS_ROOT / "suites" / "be-eht-features.json"
SUITE_ARGUMENTS = ["--suite", str(AX_SUITE)]


def configure_ax_suite():
    suite = analyze_pcap.load_suite(AX_SUITE, analyze_pcap.REPOSITORY_ROOT)
    analyze_pcap.configure_suite(
        suite,
        ANALYSIS_ROOT / "generated" / suite.suite,
    )


configure_ax_suite()

from analyze_pcap import (
    GENERATED_BEGIN,
    GENERATED_END,
    MANIFEST_PATH,
    TIMELINE_LIMIT,
    capture_map_from_manifest,
    capture_source_state,
    calculate_phy_rate,
    compressed_block_ack_records_markdown,
    has_compressed_block_ack_records,
    compact_statistics_markdown,
    decode_eht_fields,
    decode_he_fields,
    decode_legacy_fields,
    estimate_airtime,
    evaluate_evidence,
    extract_frame_timeline,
    extract_compressed_block_ack_records,
    extract_he_trigger_allocations,
    get_config_pcap_stats,
    generate_markdown_tables,
    is_generated_analysis_artifact,
    replace_generated_section,
    update_walkthrough,
    load_json_without_duplicates,
    manifest_provenance,
    main,
    parse_args,
    parse_capinfos_interfaces,
    parse_capinfos_table,
    publish_capture_manifest,
    selected_manifest_path,
    select_representative_timeline,
    timeline_filter_for_subdir,
    timeline_markdown,
    timeline_role,
    trigger_allocations_markdown,
    validate_capture_decode,
    validate_capture_metadata,
    validate_entry_binding,
    validate_evidence_checks,
)


class TimelineRoleTest(unittest.TestCase):

    def test_default_frame_exchange_timeline_limit_is_100_rows(self):
        self.assertEqual(TIMELINE_LIMIT, 100)

    def test_qos_null_is_not_described_as_payload(self):
        self.assertEqual(
            timeline_role({"frame_name": "Data: QoS Null"}),
            "Responds without MAC payload while preserving QoS control information.",
        )


class TriggerAllocationDecodeTest(unittest.TestCase):

    def test_preserves_repeated_trigger_user_info_in_field_order(self):
        result = SimpleNamespace(
            returncode=0,
            stdout=(
                "191\t0.319200000\t0\t0\t"
                "0x0000000000000003,0x0000000000000002\t"
                "0,0\t0,1\t0x0,0x2\t35,34\n"
            ),
            stderr="",
        )
        with patch("analyze_pcap.subprocess.run", return_value=result):
            rows = extract_he_trigger_allocations([
                analyze_pcap.REPOSITORY_ROOT / "capture.pcapng"
            ])
        self.assertEqual(
            [user["association_id"] for user in rows[0]["users"]],
            [3, 2],
        )
        self.assertEqual(
            [user["ru_allocation"] for user in rows[0]["users"]],
            [0, 1],
        )
        self.assertTrue(rows[0]["field_cardinality_consistent"])
        markdown = trigger_allocations_markdown(rows)
        self.assertIn("#0: AID=3, RU=0", markdown)
        self.assertIn("#1: AID=2, RU=1", markdown)

    def test_marks_mismatched_repeated_field_cardinality(self):
        result = SimpleNamespace(
            returncode=0,
            stdout="191\t0.3192\t0\t0\t1,2\t0,0\t0\t0,0\t35,35\n",
            stderr="",
        )
        with patch("analyze_pcap.subprocess.run", return_value=result):
            rows = extract_he_trigger_allocations([
                analyze_pcap.REPOSITORY_ROOT / "capture.pcapng"
            ])
        self.assertFalse(rows[0]["field_cardinality_consistent"])
        self.assertIsNone(rows[0]["users"][1]["ru_allocation"])

    def test_default_table_limit_is_100_rows(self):
        triggers = []
        for frame_number in range(101):
            triggers.append({
                "frame_number": frame_number,
                "simulation_time": "0.0",
                "trigger_type": 0,
                "users": [],
                "field_cardinality_consistent": True,
                "user_field_cardinalities": {},
            })
        markdown = trigger_allocations_markdown(triggers)
        self.assertIn("Showing the first 100 of 101 decoded Trigger frames", markdown)
        self.assertNotIn("| 100 |", markdown)


class PcapMarkdownTest(unittest.TestCase):

    def test_block_ack_timeline_prints_acknowledged_ampdu_reference(self):
        markdown = timeline_markdown([{
            "capture": "results/ap.wlan0.pcapng",
            "frame_number": 8,
            "simulation_time_s": 1.25,
            "transmitter": "00:00:00:00:00:02",
            "receiver": "00:00:00:00:00:01",
            "direction": "direct/IBSS",
            "frame_name": "Control: Block Ack (BA)",
            "retry": False,
            "sequence_number": None,
            "fragment_number": None,
            "more_fragments": False,
            "tid": None,
            "ampdu": False,
            "ampdu_reference": None,
            "acknowledged_sequence_numbers": [42, 43],
            "acknowledged_ampdu_references": ["9"],
            "phy": {"format": "Legacy/HT/VHT"},
        }])
        self.assertIn("A-MPDUs acknowledged=9", markdown)

    def test_block_ack_table_is_enabled_for_n_block_ack_configs(self):
        self.assertTrue(has_compressed_block_ack_records("block_ack", "CompressedBlockAck"))
        self.assertTrue(has_compressed_block_ack_records("block_ack", "HtImplicitBlockAck"))
        self.assertFalse(has_compressed_block_ack_records("block_ack", "StandardAck"))

    def test_extracts_acknowledged_sequences_from_compressed_block_ack_bitmap(self):
        result = SimpleNamespace(
            returncode=0,
            stdout="42\t0.300100000\t0a:aa:00:00:00:01\t10:00:00:00:00:00\t0x0004\t4094\t05:00:00:00:00:00:00:00\n",
            stderr="",
        )
        with patch("analyze_pcap.subprocess.run", return_value=result):
            rows = extract_compressed_block_ack_records([
                analyze_pcap.REPOSITORY_ROOT / "capture.pcapng"
            ])
        self.assertEqual(rows[0]["acknowledged_sequence_numbers"], [4094, 0])
        self.assertEqual(rows[0]["origin_address"], "0a:aa:00:00:00:01")
        self.assertEqual(rows[0]["destination_address"], "10:00:00:00:00:00")

    def test_compressed_block_ack_tables_are_separated_by_group_by_address(self):
        records = [
            {
                "frame_number": 2,
                "simulation_time_s": 0.2,
                "origin_address": "0a:aa:00:00:00:02",
                "destination_address": "10:00:00:00:00:00",
                "starting_sequence_number": 5,
                "bitmap": "03:00:00:00:00:00:00:00",
                "acknowledged_sequence_numbers": [5, 6],
            },
            {
                "frame_number": 1,
                "simulation_time_s": 0.1,
                "origin_address": "0a:aa:00:00:00:01",
                "destination_address": "10:00:00:00:00:00",
                "starting_sequence_number": 1,
                "bitmap": "01:00:00:00:00:00:00:00",
                "acknowledged_sequence_numbers": [1],
            },
        ]
        markdown_origin = compressed_block_ack_records_markdown(records, group_by="origin")
        idx1 = markdown_origin.find("##### Origin address: 0a:aa:00:00:00:01")
        idx2 = markdown_origin.find("##### Origin address: 0a:aa:00:00:00:02")
        self.assertGreater(idx1, -1)
        self.assertGreater(idx2, -1)
        self.assertLess(idx1, idx2)
        self.assertEqual(markdown_origin.count("|---:|---:|---:|---|---|"), 2)

        markdown_dest = compressed_block_ack_records_markdown(records, group_by="destination")
        self.assertIn("##### Destination address: 10:00:00:00:00:00", markdown_dest)
        self.assertEqual(markdown_dest.count("|---:|---:|---:|---|---|"), 1)

    def test_compressed_block_ack_table_is_limited_to_100_rows(self):
        records = [
            {
                "frame_number": frame_number,
                "simulation_time_s": 0.3,
                "destination_address": "10:00:00:00:00:00",
                "starting_sequence_number": frame_number,
                "bitmap": "01:00:00:00:00:00:00:00",
                "acknowledged_sequence_numbers": [frame_number],
            }
            for frame_number in range(101)
        ]
        markdown = compressed_block_ack_records_markdown(records)
        self.assertIn("Showing the first 100 of 101 decoded HT Compressed Block Ack frames", markdown)
        self.assertNotIn("| 100 |", markdown)

    def test_ht_implicit_block_ack_check_requires_block_ack_without_bar(self):
        config_results = {
            "UlSUHTAMpduCompressedBlockAck": {
                "global": {
                    "total": 2,
                    "stats": {
                        ("1", "9"): {"count": 1},
                    },
                    "multi_sta_block_ack_records": [],
                },
            },
        }
        checks = evaluate_evidence(config_results, "ul_multitid")
        ht_check = next(check for check in checks if check["id"] == "ht-implicit-compressed-ba")
        self.assertEqual(ht_check["status"], "PASS")
        self.assertIn("0 BAR", ht_check["evidence"])

    def test_bsr_includes_decoded_trigger_type_table(self):
        config_results = {
            "BurstyTraffic": {
                "global": {
                    "used_ap_only": True,
                    "total": 1,
                    "stats": {},
                    "timeline": [],
                    "he_trigger_allocations": [{"trigger_type": 0}],
                },
            },
        }
        manifest = {
            "session_id": "test-session",
            "tshark_version": "test-tshark",
            "_selected_manifest": {"path": "manifest.json", "sha256": "test"},
        }
        with patch("analyze_pcap.make_table_md", return_value=""), \
                patch("analyze_pcap.timeline_markdown", return_value=""), \
                patch("analyze_pcap.trigger_allocations_markdown", return_value="trigger table\n"), \
                patch("analyze_pcap.walkthrough_packet_plot_path", return_value="plot.png"):
            markdown = generate_markdown_tables(
                config_results, "bsr", [], manifest, "plot.png"
            )
        self.assertIn("#### [script] Decoded HE Trigger user allocations", markdown)


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
        with patch("analyze_pcap.get_sim_time_limit", return_value=1):
            with patch("analyze_pcap.validate_capture_decode"):
                with patch(
                    "analyze_pcap.subprocess.run", return_value=result
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


class BssColorStatisticsTest(unittest.TestCase):

    def test_groups_he_observations_by_known_bss_color(self):
        def he_observation(frame_number, bss_color, signal):
            fields = [""] * 46
            fields[0] = "0"
            fields[1] = "2"
            fields[2] = "8"
            fields[3] = "100"
            fields[4] = "20"
            fields[6] = str(signal)
            fields[23] = "1"
            fields[25] = "0"
            fields[26] = "0"
            fields[27] = "1"
            fields[28] = "2"
            fields[29] = "1"
            fields[35] = "0"
            fields[43] = str(frame_number)
            fields[44] = "1"
            fields[45] = str(bss_color)
            return "\t".join(fields)

        result = SimpleNamespace(
            returncode=0,
            stdout="\n".join([
                he_observation(1, 1, -60),
                he_observation(2, 2, -80),
            ]) + "\n",
            stderr="",
        )
        with patch("analyze_pcap.get_sim_time_limit", return_value=1), \
                patch("analyze_pcap.validate_capture_decode"), \
                patch("analyze_pcap.subprocess.run", return_value=result):
            statistics, total = get_config_pcap_stats(
                ["capture.pcapng"], "Config", "bss_coloring"
            )

        self.assertEqual(total, 2)
        self.assertEqual({key[10] for key in statistics}, {"1", "2"})
        self.assertEqual(
            {key[10]: statistic["rx_sig"] for key, statistic in statistics.items()},
            {"1": "-60.0 dBm", "2": "-80.0 dBm"},
        )
        markdown = analyze_pcap.make_table_md(statistics, total)
        self.assertIn("| BSS Color |", markdown)
        self.assertIn("| 1 | 1 | 50.00%", markdown)
        self.assertIn("| 2 | 1 | 50.00%", markdown)


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
            "# Walkthrough\n\n"
            "## [agent] **PCAP statistics**\n\nAuthored summary.\n\n"
            "## [agent] Frame exchange analysis\n\nTimeline.\n"
        )
        updated = replace_generated_section(original, "generated")
        self.assertLess(
            updated.index("generated"),
            updated.index("## [agent] Frame exchange analysis"),
        )
        self.assertIn("Authored summary.", updated)

    def test_tail_level_bundle_is_migrated_inside_pcap_section(self):
        original = (
            "# Walkthrough\n\n"
            "## [agent] PCAP statistics\n\nAuthored summary.\n\n"
            "## [agent] Frame exchange analysis\n\nTimeline.\n\n"
            f"{GENERATED_BEGIN}\nold\n{GENERATED_END}\n"
        )
        updated = replace_generated_section(original, "new")
        self.assertLess(
            updated.index(GENERATED_BEGIN),
            updated.index("## [agent] Frame exchange analysis"),
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

    def test_update_refreshes_pcap_session_and_preserves_agent_session(self):
        original = (
            "# Walkthrough\n\n"
            "<!-- BEGIN SCRIPT RESULTS SESSIONS -->\n"
            "`[script]` results sessions:\n\n"
            "- Scalar/vector: `20260725T120411Z`\n"
            "- PCAP: `20260725T230151Z`\n"
            "<!-- END SCRIPT RESULTS SESSIONS -->\n\n"
            "`[agent]` results sessions: `20260725T120411Z`.\n\n"
            "## [agent] PCAP statistics\n"
        )
        updated = update_walkthrough(
            original,
            "### [script] Generated table",
            "20260726T160000Z",
        )
        self.assertIn("- Scalar/vector: `20260725T120411Z`", updated)
        self.assertIn("- PCAP: `20260726T160000Z`", updated)
        self.assertIn(
            "`[agent]` results sessions: `20260725T120411Z`.",
            updated,
        )


class PacketPlotStorageTest(unittest.TestCase):

    def test_single_user_packet_colors_use_yellow_family(self):
        he_su = analyze_pcap.get_packet_color(
            "Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz]"
        )
        he_er_su = analyze_pcap.get_packet_color(
            "Data: QoS Data [HE-ER-SU, HE-MCS 0, 242-tone RU]"
        )
        he_mu = analyze_pcap.get_packet_color(
            "Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU]"
        )

        for color in (he_su, he_er_su):
            red, green, blue = (
                round(channel * 255)
                for channel in analyze_pcap.matplotlib.colors.to_rgb(color)
            )
            self.assertGreater(red, 150)
            self.assertGreater(green, 120)
            self.assertLess(blue, 100)
        self.assertGreater(
            analyze_pcap.matplotlib.colors.to_rgb(he_mu)[1],
            analyze_pcap.matplotlib.colors.to_rgb(he_mu)[0],
        )

    def test_stores_plot_in_shared_result_session(self):
        with tempfile.TemporaryDirectory() as directory:
            example_root = Path(directory)
            result_session = example_root / "results" / "20260726T120000Z"
            statistics = {
                ("0", "0", False): {
                    "count": 1,
                    "airtime_pct": 100.0,
                }
            }
            config_results = {
                "Baseline": {
                    "global": {
                        "total": 1,
                        "stats": statistics,
                    }
                }
            }
            packet_type = analyze_pcap.unpack_key_to_name(
                next(iter(statistics))
            )
            with patch("analyze_pcap.EXAMPLE_ROOT", example_root):
                plot_path = analyze_pcap.generate_stacked_bar_plot(
                    config_results,
                    "mac_features/dynamic_fragmentation",
                    {packet_type: "#336699"},
                    result_session,
                )

            self.assertEqual(
                plot_path,
                result_session / "packet_statistics.png",
            )
            self.assertTrue(plot_path.is_file())

    def test_walkthrough_references_result_session_copy(self):
        with patch(
            "analyze_pcap.EXAMPLE_ROOT",
            Path("/repository/examples/ieee80211ax"),
        ):
            self.assertEqual(
                analyze_pcap.walkthrough_packet_plot_path(
                    "ul_ofdma",
                    Path(
                        "/repository/examples/ieee80211ax/ul_ofdma/results/"
                        "20260726T120000Z/packet_statistics.png"
                    ),
                ),
                "results/20260726T120000Z/packet_statistics.png",
            )
            self.assertEqual(
                analyze_pcap.walkthrough_packet_plot_path(
                    "mac_features/dynamic_fragmentation",
                    Path(
                        "/repository/examples/ieee80211ax/mac_features/"
                        "dynamic_fragmentation/results/20260726T120000Z/"
                        "packet_statistics.png"
                    ),
                ),
                "results/20260726T120000Z/packet_statistics.png",
            )


class CaptureValidationTest(unittest.TestCase):

    def test_shared_result_index_rejects_capture_from_another_run(self):
        with tempfile.TemporaryDirectory() as directory:
            result_dir = Path(directory)
            selected = result_dir / "Config-#0Network.ap.wlan[0].pcapng"
            selected.touch()
            self.assertEqual(
                analyze_pcap.discover_run_captures(
                    result_dir, "Config", 0
                ),
                [selected],
            )
            (result_dir / "Config-#1Network.ap.wlan[0].pcapng").touch()
            with self.assertRaisesRegex(
                RuntimeError, "another run"
            ):
                analyze_pcap.discover_run_captures(
                    result_dir, "Config", 0
                )

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
            with patch("analyze_pcap.subprocess.run", return_value=result):
                with self.assertRaisesRegex(RuntimeError, "exit 2: bad capture"):
                    validate_capture_decode(capture)

    def test_first_frame_requires_all_decisive_decode_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory) / "capture.pcapng"
            capture.write_bytes(b"capture")
            result = SimpleNamespace(returncode=0, stdout="1\t0\t\n", stderr="")
            with patch("analyze_pcap.subprocess.run", return_value=result):
                with self.assertRaisesRegex(RuntimeError, "First frame lacks"):
                    validate_capture_decode(capture)

    def test_feature_timeline_filters_select_decisive_frames(self):
        self.assertIn("wlan.fc.subtype == 10", timeline_filter_for_subdir("twt"))
        self.assertIn(
            "wlan.fc.subtype == 2", timeline_filter_for_subdir("dl_ofdma_sched")
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
        self.assertIn("| 7 |", markdown)
        self.assertIn("capture `ap.wlan0.pcapng`", markdown)
        self.assertIn(
            "direction=from DS, retry=1, seq=42, frag=0, more-frag=0, "
            "TID=5, A-MPDU=9",
            markdown,
        )
        self.assertIn(
            "QoS Data [HE-MU, HE-MCS 3, 52-tone RU, GI 0.8 us, LDPC, A-MPDU]",
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
                "analyze_pcap.subprocess.run",
                side_effect=[validation, decode],
            ):
                timeline = extract_frame_timeline(
                    [capture], "dl_ofdma_sched", limit=2
                )
        self.assertEqual(
            [frame["frame_number"] for frame in timeline], [7, 8]
        )
        self.assertEqual(timeline[1]["direction"], "to DS")
        self.assertTrue(timeline[1]["more_fragments"])
        self.assertEqual(timeline[1]["phy"]["format"], "HE-MU")

    def test_timeline_parser_correlates_block_ack_bitmap_to_ampdu(self):
        with tempfile.TemporaryDirectory(
            dir=Path(__file__).resolve().parent
        ) as directory:
            capture = Path(directory) / "ap.wlan0.pcapng"
            capture.write_bytes(b"capture")
            validation = SimpleNamespace(
                returncode=0, stdout="1\t0\t2\n", stderr=""
            )
            data = [
                "7", "1.000", "00:01", "00:02", "", "", "2", "8", "0",
                "42", "0", "5", "1", "9", "", "", "", "", "", "", "",
                "0", "0", "0", "", "", "", "", "", "", "", "", "", "",
            ]
            block_ack = [
                "8", "1.001", "00:02", "00:01", "", "", "1", "9", "0",
                "", "", "", "0", "", "", "", "", "", "", "", "", "",
                "0", "0", "0", "", "", "", "", "", "", "", "0x0004", "42",
                "03:00:00:00:00:00:00:00",
            ]
            decode = SimpleNamespace(
                returncode=0,
                stdout="\n".join(("\t".join(data), "\t".join(block_ack))) + "\n",
                stderr="",
            )
            with patch(
                "analyze_pcap.subprocess.run",
                side_effect=[validation, decode],
            ):
                timeline = extract_frame_timeline([capture], "block_ack", limit=2)
        block_ack_row = next(row for row in timeline if row["frame_subtype"] == "9")
        self.assertEqual(block_ack_row["acknowledged_sequence_numbers"], [42, 43])
        self.assertEqual(block_ack_row["acknowledged_ampdu_references"], ["9"])

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
            "Observation point: Access Point (AP) wireless interfaces.",
            markdown,
        )
        self.assertIn("| `Treatment` | `wlan.fc.type == 2` | 4 |", markdown)
        self.assertIn("QoS Data [HE-SU", markdown)
        self.assertIn("Not delivery or de-duplicated transmissions", markdown)


class DlOfdmaEvidenceTest(unittest.TestCase):

    def test_per_flow_attribution_is_not_run_without_asymmetric_configs(self):
        key = ("2", "8", "HE-MU", "HE-MCS 1", "106-tone RU",
               "3.2 us", "1", "LDPC", True, False)
        config_results = {
            "EqualSizedRUs_fBW": {
                "global": {"stats": {key: {"count": 2}}, "total": 2},
            }
        }

        checks = {
            check["id"]: check
            for check in evaluate_evidence(config_results, "dl_ofdma_sched")
        }

        check = checks["dl-ofdma-per-user-attribution"]
        self.assertEqual(check["status"], "NOT RUN")
        self.assertIn("No asymmetric backlog/HoL", check["evidence"])

    def test_he_mu_qos_ampdu_and_per_flow_attribution_pass(self):
        key = ("2", "8", "HE-MU", "HE-MCS 1", "52-tone RU",
               "3.2 us", "1", "LDPC", True, False)
        config_results = {}
        for config_name in ("BacklogBased", "HoLMinDelay", "BacklogBased2_0ms", "HoLMinDelay2_0ms", "BacklogBased4ms", "HoLMinDelay4ms", "BacklogBased3ms", "HoLMinDelay3ms", "BacklogBased2_5ms", "HoLMinDelay2_5ms", "BacklogBased1_5ms", "HoLMinDelay1_5ms"):
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
            for check in evaluate_evidence(config_results, "dl_ofdma_asym")
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

    def setUp(self):
        configure_ax_suite()

    def test_suite_is_required_and_configured_before_scenario_validation(self):
        with self.assertRaises(SystemExit) as help_exit:
            parse_args(["--help"])
        self.assertEqual(help_exit.exception.code, 0)
        with self.assertRaises(SystemExit):
            parse_args(["--reuse"])
        with self.assertRaises(SystemExit):
            parse_args([
                "--suite",
                str(BE_SUITE),
                "--reuse",
                "--subdir",
                "twt",
            ])
        self.assertEqual(analyze_pcap.SUITE_NAME, "be-eht-features")
        self.assertEqual(
            set(analyze_pcap.subdirs_configs),
            {"eht_features"},
        )

    def test_ax_and_eht_suites_bind_descriptor_output_and_marker(self):
        expectations = (
            (AX_SUITE, "ax", "ieee80211ax-pcap-statistics"),
            (
                BE_SUITE,
                "be-eht-features",
                "ieee80211-pcap-statistics",
            ),
        )
        for descriptor, suite_name, marker in expectations:
            args = parse_args([
                "--suite",
                str(descriptor),
                "--reuse",
                "--subdir",
                next(iter(
                    analyze_pcap.load_suite(
                        descriptor, analyze_pcap.REPOSITORY_ROOT
                    ).scenarios
                )),
            ])
            self.assertEqual(args.suite, descriptor)
            self.assertEqual(analyze_pcap.SUITE_NAME, suite_name)
            self.assertEqual(
                analyze_pcap.SUITE_DESCRIPTOR_PATH,
                descriptor.resolve(),
            )
            self.assertEqual(
                analyze_pcap.ANALYSIS_OUTPUT_DIR,
                ANALYSIS_ROOT / "generated" / suite_name,
            )
            self.assertEqual(analyze_pcap.GENERATED_MARKER, marker)
            provenance = analyze_pcap.suite_provenance()
            self.assertEqual(
                provenance["path"],
                descriptor.relative_to(
                    analyze_pcap.REPOSITORY_ROOT
                ).as_posix(),
            )
            self.assertEqual(len(provenance["sha256"]), 64)

    def test_new_manifest_records_selected_suite_descriptor(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = Path(analyze_pcap.__file__)
            history = root / "history.json"
            with (
                patch(
                    "analyze_pcap.capture_source_state",
                    return_value=("revision", "source-hash"),
                ),
                patch("analyze_pcap.inet_library_path", return_value=library),
                patch("analyze_pcap.tshark_version", return_value="TShark"),
                patch("analyze_pcap.capinfos_version", return_value="Capinfos"),
                patch("analyze_pcap._history_path", return_value=history),
                patch(
                    "analyze_pcap.campaign_result_directory",
                    return_value=root / "session" / "Baseline",
                ),
                patch(
                    "analyze_pcap.index_simulation_result",
                    return_value={"subdir": "twt", "config": "Baseline"},
                ),
                patch("analyze_pcap.publish_capture_manifest"),
                patch.dict(
                    analyze_pcap.subdirs_configs,
                    {"twt": ["Baseline"]},
                    clear=True,
                ),
            ):
                (root / "session" / "Baseline").mkdir(parents=True)
                manifest = analyze_pcap.build_capture_manifest(
                    ["twt"], 0, "20260726T120000Z"
                )
        self.assertEqual(manifest["suite"], "ax")
        self.assertEqual(
            manifest["suite_descriptor"]["path"],
            "examples/ieee80211/analysis/suites/ax.json",
        )
        self.assertEqual(len(manifest["suite_descriptor"]["sha256"]), 64)
        self.assertNotIn("schema_version", manifest)
        self.assertNotIn("result_directory", manifest["entries"][0])
        self.assertNotIn("scalar_file", manifest["entries"][0])

    def test_shared_analyzer_has_no_ax_analysis_core_dependency(self):
        source = Path(analyze_pcap.__file__).read_text(encoding="utf-8")
        self.assertNotIn("analysis_core", source)
        self.assertNotIn("ieee80211ax/analysis", source)

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
        manifest = {"session_id": "20260725T120000Z"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            latest = root / "latest.json"
            history = root / "history"
            with (
                patch("analyze_pcap.MANIFEST_PATH", latest),
                patch("analyze_pcap.MANIFEST_HISTORY_DIR", history),
            ):
                published = publish_capture_manifest(manifest)
                self.assertEqual(json.loads(published.read_text()), manifest)
                self.assertEqual(latest.read_text(), published.read_text())
                with self.assertRaises(FileExistsError):
                    publish_capture_manifest(manifest)
                self.assertEqual(json.loads(latest.read_text()), manifest)

    def test_walkthrough_update_is_explicit_and_disabled_by_default(self):
        with patch(
            "sys.argv",
            ["analyze_pcap.py", *SUITE_ARGUMENTS, "--reuse", "--subdir", "twt"],
        ):
            args = parse_args()
        self.assertFalse(args.update_walkthrough)
        self.assertFalse(args.capture_only)
        with patch(
            "sys.argv",
            [
                "analyze_pcap.py",
                *SUITE_ARGUMENTS,
                "--reuse",
                "--subdir",
                "twt",
                "--update-walkthrough",
            ],
        ):
            self.assertTrue(parse_args().update_walkthrough)

    def test_capture_only_stops_before_analysis_and_walkthrough_updates(self):
        manifest = {"session_id": "20260726T120000Z"}
        with (
            patch(
                "sys.argv",
                [
                    "analyze_pcap.py",
                    *SUITE_ARGUMENTS,
                    "--index",
                    "--capture-only",
                    "--subdir",
                    "twt",
                    "--session-id",
                    manifest["session_id"],
                ],
            ),
            patch(
                "analyze_pcap.build_capture_manifest",
                return_value=manifest,
            ) as build,
            patch("analyze_pcap.analyze_subdirectory") as analyze,
            patch("analyze_pcap.update_walkthrough_file") as update,
        ):
            main()
        build.assert_called_once_with(["twt"], 0, manifest["session_id"])
        analyze.assert_not_called()
        update.assert_not_called()

    def test_source_digest_includes_head_diff_and_untracked_content(self):
        with tempfile.TemporaryDirectory(
            dir=Path(__file__).resolve().parent
        ) as directory:
            source = Path(directory) / "input.ned"
            source.write_text("first")
            relative = source.relative_to(
                analyze_pcap.REPOSITORY_ROOT
            ).as_posix()

            def digest():
                with (
                    patch(
                        "analyze_pcap.command_output",
                        side_effect=["revision", relative],
                    ),
                    patch(
                        "analyze_pcap.subprocess.run",
                        return_value=SimpleNamespace(stdout=b"tracked diff"),
                    ) as run,
                ):
                    result = capture_source_state()
                command = run.call_args.args[0]
                self.assertIn("HEAD", command)
                self.assertIn(
                    "examples/ieee80211/analysis/analyze_pcap.py",
                    command,
                )
                self.assertIn(
                    "examples/ieee80211/analysis/inet_wifi_analysis",
                    command,
                )
                self.assertIn(
                    "examples/ieee80211/analysis/suites/ax.json",
                    command,
                )
                self.assertFalse(
                    any("/generated/" in argument for argument in command)
                )
                return result

            first = digest()
            source.write_text("second")
            second = digest()
            self.assertEqual(first[0], "revision")
            self.assertNotEqual(first[1], second[1])

            generated = Path(directory) / "packet_statistics.png.json"
            generated.write_text("first")
            generated_relative = generated.relative_to(
                analyze_pcap.REPOSITORY_ROOT
            ).as_posix()

            def generated_digest():
                with (
                    patch(
                        "analyze_pcap.command_output",
                        side_effect=["revision", generated_relative],
                    ),
                    patch(
                        "analyze_pcap.subprocess.run",
                        return_value=SimpleNamespace(stdout=b"tracked diff"),
                    ) as run,
                ):
                    result = capture_source_state()
                self.assertIn(
                    ":(exclude)examples/ieee80211ax/**/*.png.json",
                    run.call_args.args[0],
                )
                return result

            generated_first = generated_digest()
            generated.write_text("second")
            generated_second = generated_digest()
            self.assertEqual(generated_first, generated_second)
            self.assertTrue(
                is_generated_analysis_artifact(
                    "examples/ieee80211ax/twt/packet_statistics.png.json",
                    "examples/ieee80211ax",
                )
            )

    def test_session_selection_and_duplicate_keys(self):
        self.assertEqual(selected_manifest_path(), MANIFEST_PATH)
        self.assertTrue(
            str(selected_manifest_path("20260725T120000Z")).endswith(
                "capture_manifests/20260725T120000Z.json"
            )
        )
        with tempfile.TemporaryDirectory(
            dir=Path(__file__).resolve().parent
        ) as directory:
            duplicate = Path(directory) / "duplicate.json"
            duplicate.write_text('{"session_id": "first", "session_id": "second"}')
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
                "subdir": "dl_ofdma_sched",
                "config": "EqualSizedRUs_fBW",
                "run_number": 0,
                "captures": [{"path": "examples/capture.pcapng"}],
            }]
        }
        mapping = capture_map_from_manifest(manifest, ["dl_ofdma_sched"], 0)
        self.assertEqual(
            mapping["dl_ofdma_sched"]["EqualSizedRUs_fBW"][0].name,
            "capture.pcapng",
        )

    def test_manifest_entry_binds_artifacts_to_session_config_run_and_seed(self):
        base = (
            "examples/ieee80211ax/dl_ofdma_sched/results/"
            "20260725T120000Z/EqualSizedRUs_fBW"
        )
        entry = {
            "subdir": "dl_ofdma_sched",
            "config": "EqualSizedRUs_fBW",
            "run_number": 0,
            "seed_set": 7,
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
            "examples/ieee80211ax/dl_ofdma_sched/results/"
            "20260724T120000Z/EqualSizedRUs_fBW/ap.wlan0.pcap"
        )
        entry["seed_set"] = 8
        errors = validate_entry_binding(entry, "20260725T120000Z")
        self.assertTrue(any("capture path" in error for error in errors))
        self.assertTrue(any("seedset" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
