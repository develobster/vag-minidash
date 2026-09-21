import json
from pathlib import Path
import tempfile
import unittest

from minidash_live.__main__ import main
from minidash_live.profiles import load_profile, match_catalog, resolve_signal, signals


ROOT = Path(__file__).resolve().parents[1]


class ProbeTests(unittest.TestCase):
    def test_handoff_catalog_is_complete_and_raw_only(self):
        expected = {
            "j623-czpa-5na907115d.json": {"F405", "F40F", "F43C", "F40D", "16F3", "F433", "11BD", "15B7", "1632", "11F0", "11F1", "11EF", "11DC", "11DD", "11DA", "159F", "11D7", "15CD", "1065", "149E", "128A"},
            "j743-dq500-0dl300011p.json": {"2104", "102B", "102C", "102A", "028D", "38B2", "100A", "380A", "380B", "380C", "380D", "380E", "380F", "3816", "382E", "382F", "3830", "3831", "3834", "3835", "3838", "3839", "383A", "383D", "383E", "38EA", "38EB", "38EC", "38ED"},
            "j104-esc-5q0614517ce.json": {"1800", "1801", "1802", "1803", "1804", "1816"},
            "j492-haldex-0cq907554h.json": {"028D", "2BE4", "2BF1", "2BE6", "2BE7", "2BE9", "2BF3"},
        }
        directory = ROOT / "data/ecu_profiles"
        for filename, dids in expected.items():
            profile = load_profile(directory / filename)
            self.assertEqual({x["did"] for x in signals(profile)}, dids)
            self.assertTrue(all(x["raw"]["decoder"] is None for x in signals(profile)))
        engine = load_profile(directory / "j623-czpa-5na907115d.json")
        self.assertEqual(resolve_signal(engine, "1632")["validation_status"], "UNSUPPORTED")

    def test_exact_fingerprint_match_does_not_use_vehicle_name(self):
        paths = sorted((ROOT / "data/ecu_profiles").glob("*.json"))
        observed = {"sw_part": "5NA907115D", "sw_version": "0003", "hw_part": "06L907309B", "hw_version": "H31", "asam_id": "EV_ECM20TFS0115NA907115D", "asam_version": "001004"}
        self.assertEqual([x["id"] for x in match_catalog(paths, observed)], ["j623-czpa-5na907115d-0003"])
        observed["asam_version"] = "001003"  # ODX container revision is not ECU-reported ASAM version.
        self.assertEqual(match_catalog(paths, observed), [])

    def test_replay_probe_records_complete_exchange_and_unknown_did(self):
        with tempfile.TemporaryDirectory() as work:
            work = Path(work)
            fixture = ROOT / "tests/fixtures/synthetic-uds-replay.json"
            config = work / "connection.json"
            config.write_text(json.dumps({"transport": {"kind": "replay", "fixture": str(fixture)}}), encoding="utf-8")
            trace = work / "trace.jsonl"
            status = main(["probe", "--config", str(config), "--ecu", "engine", "--did", "F405", "--did", "F999", "--trace", str(trace)])
            self.assertEqual(status, 0)
            events = [json.loads(line) for line in trace.read_text(encoding="utf-8").splitlines()]
            exchanges = [x for x in events if x["kind"] == "uds_exchange"]
            self.assertEqual([(x["did"], x["request"], x["response"]) for x in exchanges], [("F405", "22f405", "62f405aabb"), ("F999", "22f999", "62f99900")])
            self.assertEqual(exchanges[0]["decoded"]["quality"], "raw_only")
            self.assertEqual(exchanges[1]["decoded"]["canonical_name"], "unknown.did.F999")
            self.assertIn("selected_profile_fingerprint", exchanges[0])
            self.assertNotIn("ecu_fingerprint", exchanges[0])
            self.assertFalse(exchanges[0]["identity_verified"])
            self.assertFalse(any(x["kind"] == "error" for x in events))

    def test_trace_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as work:
            work = Path(work)
            config = work / "connection.json"
            config.write_text(json.dumps({"transport": {"kind": "replay", "fixture": "missing.json"}}), encoding="utf-8")
            trace = work / "existing.jsonl"
            trace.write_text("preserve", encoding="utf-8")
            with self.assertRaises(SystemExit) as result:
                main(["probe", "--config", str(config), "--ecu", "engine", "--did", "F405", "--trace", str(trace)])
            self.assertEqual(result.exception.code, 2)
            self.assertEqual(trace.read_text(encoding="utf-8"), "preserve")
