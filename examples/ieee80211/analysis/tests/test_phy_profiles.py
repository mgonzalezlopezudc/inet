import json
import sys
import unittest
from pathlib import Path


ANALYSIS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ANALYSIS_ROOT))

from inet_wifi_analysis import (
    decode_eht_radiotap,
    decode_phy_observation,
    load_suite,
)


class PhyProfilesTest(unittest.TestCase):

    def test_he_profile_is_discriminated(self):
        observation = decode_phy_observation({
            "radiotap.present.he": "1",
            "radiotap.he.data_1.ppdu_format": "2",
            "radiotap.he.data_3.data_mcs": "3",
            "radiotap.he.data_3.coding": "1",
            "radiotap.he.data_5.data_bw_ru_allocation": "5",
            "radiotap.he.data_5.gi": "0",
            "radiotap.he.data_6.nsts": "2",
        })
        self.assertEqual(observation.generation, "he")
        self.assertEqual(observation.ppdu_format, "HE-MU")
        self.assertEqual(observation.ru, "52-tone RU")
        self.assertEqual(observation.nss, 2)

    def test_eht_known_bits_gate_values(self):
        observation = decode_phy_observation({
            "radiotap.present.eht": "1",
            "radiotap.eht.known": "0",
            "radiotap.u_sig.common": "0",
            "radiotap.eht.user_info": "0",
            "radiotap.eht.data_0.gi": "0",
            "radiotap.eht.user_info.mcs": "13",
            "radiotap.eht.user_info.nss": "1",
        })
        self.assertEqual(observation.generation, "eht")
        self.assertIsNone(observation.mcs)
        self.assertIsNone(observation.nss)
        self.assertIsNone(observation.guard_interval_us)
        self.assertIsNone(observation.bandwidth_mhz)

    def test_eht_su_recorder_projection_round_trips(self):
        observation = decode_phy_observation({
            "radiotap.present.eht": "1",
            "radiotap.eht.known": "0x00080004",
            "radiotap.u_sig.common": "0x00018003",
            "radiotap.eht.data_0.gi": "1",
            "radiotap.eht.user_info": "0x01d00096",
            "radiotap.eht.user_info.mcs": "13",
            "radiotap.eht.user_info.coding": "0",
            "radiotap.eht.user_info.nss": "1",
        })
        self.assertEqual(observation.bandwidth_mhz, 160)
        self.assertEqual(observation.guard_interval_us, 1.6)
        self.assertEqual(observation.mcs, 13)
        self.assertEqual(observation.nss, 2)
        self.assertEqual(observation.coding, "BCC")
        self.assertFalse(observation.limitations)

    def test_eht_320_is_supported_only_when_u_sig_supplies_variant(self):
        observation = decode_phy_observation({
            "radiotap.present.eht": "1",
            "radiotap.eht.known": "0x4",
            "radiotap.u_sig.common": hex(0x3 | (4 << 15)),
            "radiotap.eht.data_0.gi": "0",
        })
        self.assertEqual(observation.bandwidth_mhz, 320)

    def test_eht_selects_the_unique_captured_user(self):
        observation = decode_phy_observation({
            "radiotap.present.eht": "1",
            "radiotap.eht.known": "0x4",
            "radiotap.u_sig.common": "0",
            "radiotap.eht.user_info": "0x01d00016,0x00280096",
        })
        self.assertEqual(observation.mcs, 2)
        self.assertEqual(observation.nss, 1)
        self.assertEqual(observation.coding, "LDPC")

    def test_eht_suppresses_user_facts_without_captured_user_marker(self):
        observation = decode_phy_observation({
            "radiotap.present.eht": "1",
            "radiotap.eht.known": "0x4",
            "radiotap.u_sig.common": "0",
            "radiotap.eht.user_info": "0x01d00016",
        })
        self.assertIsNone(observation.mcs)
        self.assertIsNone(observation.nss)
        self.assertIsNone(observation.coding)

    def test_eht_suppresses_ambiguous_captured_user_markers(self):
        observation = decode_phy_observation({
            "radiotap.present.eht": "1",
            "radiotap.eht.known": "0x4",
            "radiotap.u_sig.common": "0",
            "radiotap.eht.user_info": "0x01d00096,0x00280096",
        })
        self.assertIsNone(observation.mcs)
        self.assertIsNone(observation.nss)
        self.assertIsNone(observation.coding)

    def test_raw_radiotap_fallback_decodes_recorder_layout(self):
        # Header prefix from a focused EhtFeatures capture. TShark 4.6.4
        # exposes the extended present word but skips type-33/34 fields.
        frame = bytes.fromhex(
            "000050000a840080060000001000d917000014000000000001"
            "000000000000000000000004000800000100000000000000"
            "000000000000000000000000000000000000000000000000"
            "0000009600d000"
        )
        decoded = decode_eht_radiotap(frame)
        self.assertIsNotNone(decoded)
        observation = decode_phy_observation(decoded)
        self.assertEqual(observation.mcs, 13)
        self.assertEqual(observation.nss, 1)
        self.assertEqual(observation.guard_interval_us, 3.2)
        self.assertIsNone(observation.bandwidth_mhz)


class SuiteTest(unittest.TestCase):

    def test_eht_features_suite_loads(self):
        repository_root = ANALYSIS_ROOT.parents[2]
        suite = load_suite(
            ANALYSIS_ROOT / "suites" / "be-eht-features.json",
            repository_root,
        )
        self.assertEqual(suite.suite, "be-eht-features")
        self.assertIn("eht_features", suite.scenarios)

    def test_suite_rejects_empty_capture_patterns(self):
        path = ANALYSIS_ROOT / "suites" / "ax.json"
        document = json.loads(path.read_text())
        document["capture"]["interface_patterns"] = []
        temporary = ANALYSIS_ROOT / "tests" / "_invalid_suite.json"
        try:
            temporary.write_text(json.dumps(document))
            with self.assertRaisesRegex(ValueError, "interface_patterns"):
                load_suite(temporary, ANALYSIS_ROOT.parents[2])
        finally:
            temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()
