import json
from pathlib import Path
import tempfile
import unittest

from minidash_live.__main__ import FORBIDDEN_CONFIG_KEYS, _identity_probes
from minidash_live.__main__ import _read_json, _walk_keys, validate_runtime_config


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "config/examples/tiguan-vnci-engine-read-only.template.json"


class FirstLiveConfigTests(unittest.TestCase):
    def test_committed_template_is_minimal_read_only_and_fail_closed(self):
        config = _read_json(TEMPLATE)
        self.assertEqual(set(config["ecus"]), {"engine"})
        self.assertNotIn("vehicle_identity", config)
        self.assertEqual(set(_walk_keys(config)) & FORBIDDEN_CONFIG_KEYS, set())
        probes = _identity_probes(config["ecus"]["engine"])
        self.assertEqual([(item.did, item.target, item.encoding) for item in probes],
                         [(0xF190, "vehicle.vin", "ascii")])
        self.assertNotIn("identity", config["ecus"]["engine"])
        self.assertNotIn("poll_periods", config)
        report = validate_runtime_config(TEMPLATE, check_files=False)
        self.assertFalse(report["valid"])
        self.assertTrue(report["read_only"])

    def test_evidenced_values_hydrate_template_through_runtime_parser(self):
        config = _read_json(TEMPLATE)
        config["transport"]["dll"] = str(Path(tempfile.gettempdir()) / "synthetic-driver.dll")
        config["ecus"]["engine"].update({
            "txid": "700",
            "rxid": "708",
            "bitrate": 500000,
            "address_evidence": "synthetic parser test; not vehicle evidence",
        })
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "connection.json"
            path.write_text(json.dumps(config), encoding="utf-8")
            report = validate_runtime_config(path, check_files=False)
        self.assertTrue(report["valid"], report["errors"])
        self.assertEqual(report["ecu_roles"], ["engine"])
        self.assertTrue(report["read_only"])

    def test_forbidden_operation_key_fails_config_preflight(self):
        config = _read_json(TEMPLATE)
        config["security_access"] = {"level": 1}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "unsafe.json"
            path.write_text(json.dumps(config), encoding="utf-8")
            report = validate_runtime_config(path, check_files=False)
        self.assertFalse(report["valid"])
        self.assertFalse(report["read_only"])
        self.assertIn("security_access", " ".join(report["errors"]))


if __name__ == "__main__":
    unittest.main()
