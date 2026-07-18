import unittest

from analyze_pcap_types import (
    GENERATED_BEGIN,
    GENERATED_END,
    decode_he_fields,
    estimate_airtime,
    replace_generated_section,
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


class GeneratedSectionTest(unittest.TestCase):

    def test_marker_bounded_update_preserves_manual_tail(self):
        original = f"Manual prefix\n\n{GENERATED_BEGIN}\nold\n{GENERATED_END}\n\nManual tail\n"
        updated = replace_generated_section(original, "new")
        self.assertEqual(
            updated,
            f"Manual prefix\n\n{GENERATED_BEGIN}\nnew\n{GENERATED_END}\n\nManual tail\n",
        )

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


if __name__ == "__main__":
    unittest.main()
