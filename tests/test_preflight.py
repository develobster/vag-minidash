import json
import tempfile
import unittest
from pathlib import Path

from minidash_live.__main__ import (
    FORBIDDEN_CONFIG_KEYS,
    _identity_probes,
    _read_json,
    _walk_keys,
    validate_runtime_config,
)

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "config/examples/tiguan-vnci-engine-read-only.template.json"


class FirstLiveConfigTests(unittest.TestCase):
    def test_committed_template_is_evidence_backed_read_only_and_fail_closed_without_dll(self):
        config = _read_json(TEMPLATE)
        self.assertEqual(set(config["ecus"]), {"engine"})
        self.assertNotIn("vehicle_identity", config)
        self.assertEqual(set(_walk_keys(config)) & FORBIDDEN_CONFIG_KEYS, set())
        engine = config["ecus"]["engine"]
        self.assertEqual((engine["txid"], engine["rxid"], engine["bitrate"]),
                         ("7E0", "7E8", 500000))
        self.assertEqual((engine["can_id_format"], engine["isotp_addressing"]),
                         ("11bit", "normal"))
        self.assertIn("CONFIRMED REAL VEHICLE", engine["address_evidence"])
        probes = _identity_probes(config["ecus"]["engine"])
        self.assertEqual([(item.did, item.target, item.encoding) for item in probes],
                         [(0xF190, "vehicle.vin", "ascii")])
        self.assertNotIn("identity", config["ecus"]["engine"])
        self.assertEqual(config["poll_periods"], {"engine.coolant_temperature": 2.0})
        report = validate_runtime_config(TEMPLATE, check_files=False)
        self.assertFalse(report["valid"])
        self.assertTrue(report["read_only"])
        self.assertEqual(report["errors"],
                         ["ECU engine: J2534 transport needs an absolute FunctionLibrary DLL path"])

    def test_evidenced_values_hydrate_template_through_runtime_parser(self):
        config = _read_json(TEMPLATE)
        config["transport"]["dll"] = str(Path(tempfile.gettempdir()) / "synthetic-driver.dll")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "connection.json"
            path.write_text(json.dumps(config), encoding="utf-8")
            report = validate_runtime_config(path, check_files=False)
        self.assertTrue(report["valid"], report["errors"])
        self.assertEqual(report["ecu_roles"], ["engine"])
        self.assertTrue(report["read_only"])
        self.assertEqual(report["endpoints"], [{
            "role": "engine",
            "transport": "j2534",
            "protocol": "ISO15765",
            "txid": "7E0",
            "rxid": "7E8",
            "bitrate": 500000,
            "can_id_format": "11bit",
            "isotp_addressing": "normal",
            "address_evidence": config["ecus"]["engine"]["address_evidence"],
        }])

    def test_unsupported_addressing_mode_fails_preflight(self):
        config = _read_json(TEMPLATE)
        config["transport"]["dll"] = str(
            Path(tempfile.gettempdir()) / "synthetic-driver.dll")
        config["ecus"]["engine"]["isotp_addressing"] = "extended"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "connection.json"
            path.write_text(json.dumps(config), encoding="utf-8")
            report = validate_runtime_config(path, check_files=False)
        self.assertFalse(report["valid"])
        self.assertIn("Only normal ISO-TP addressing is supported",
                      " ".join(report["errors"]))

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
