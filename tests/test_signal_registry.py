import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path

from minidash_live.dbc import DbcAdapter, load_cantools_database
from minidash_live.mcd_import import import_mcd_mwb
from minidash_live.registry import (
    ConflictingDefinitionError, DuplicateDefinitionError, SignalRegistry,
    UnsupportedVariant, load_registry,
)
from minidash_live.runtime import SignalStore


ROOT = Path(__file__).resolve().parents[1]


class FakeMessage:
    name = "Motor_12"
    is_extended_frame = False
    def decode(self, data, **kwargs):
        if len(data) != 8:
            raise ValueError("length")
        return {"MO_Drehzahl_01": int.from_bytes(data[2:4], "little") * .25}


class FakeDatabase:
    def get_message_by_frame_id(self, frame_id):
        if frame_id != 0x0a8:
            raise KeyError(frame_id)
        return FakeMessage()


class SignalRegistryTests(unittest.TestCase):
    def setUp(self):
        self.opendbc = ROOT / "data/external/opendbc-vw-mqb-map.json"
        self.haldex = ROOT / "data/external/community-haldex-gen5.json"

    def test_provenance_and_applicability(self):
        registry = load_registry(self.haldex)
        context = {"manufacturer":"Volkswagen", "platform":"MQB",
            "part_number":"0CQ907554H", "software_version":"7083",
            "hardware_part_number":"0CQ525130", "hardware_version":"H52",
            "asam_id":"EV_AllWheelContrHA1VW37X", "asam_version":"004013"}
        definition = registry.require_uds("awd", 0x028d, context)
        self.assertEqual(definition.provenance.confidence, "candidate")
        self.assertFalse(definition.provenance.vehicle_tested)
        with self.assertRaises(UnsupportedVariant):
            registry.require_uds("awd", 0x028d,
                {"manufacturer":"Volkswagen", "platform":"MQB",
                 "part_number":"0CQ907554J", "software_version":"7083",
                 "hardware_part_number":"0CQ525130", "hardware_version":"H52",
                 "asam_id":"EV_AllWheelContrHA1VW37X", "asam_version":"004013"})

    def test_did_decode_unknown_and_invalid(self):
        registry = load_registry(self.haldex)
        context = {"manufacturer":"Volkswagen", "platform":"MQB",
            "part_number":"0CQ907554H", "software_version":"7083",
            "hardware_part_number":"0CQ525130", "hardware_version":"H52",
            "asam_id":"EV_AllWheelContrHA1VW37X", "asam_version":"004013"}
        decoded = registry.decode_uds("awd", 0x2bf1, bytes.fromhex("8563"), context)
        self.assertAlmostEqual(decoded["value"], 27.1)
        self.assertEqual(decoded["quality"], "candidate")
        self.assertEqual(registry.decode_uds("awd", 0xffff, b"\x01", context)["quality"], "raw_only")
        self.assertEqual(registry.decode_uds("awd", 0x2bf1, b"\x01", context)["quality"], "invalid_length")

    def test_duplicate_and_conflicting_definitions(self):
        definition = load_registry(self.haldex).definitions[0]
        with self.assertRaises(DuplicateDefinitionError):
            SignalRegistry([definition, definition])
        other = load_registry(self.haldex).definitions[1]
        conflicting = type(definition)(id="conflict.test", canonical_name=other.canonical_name,
            ecu=definition.ecu, source=definition.source, provenance=definition.provenance,
            applicability=definition.applicability, decoder=definition.decoder,
            unit=definition.unit, ttl=definition.ttl)
        with self.assertRaises(ConflictingDefinitionError):
            SignalRegistry([definition, conflicting])

    def test_dbc_adapter_normalizes_and_store_expires(self):
        registry = load_registry(self.opendbc)
        now = [0.0]
        store = SignalStore(lambda: now[0])
        adapter = DbcAdapter(FakeDatabase(), "opendbc/vw_mqb", registry, store,
                             context={"platform": "MQB"})
        data = bytearray(8)
        data[2:4] = (3200).to_bytes(2, "little")
        self.assertEqual(adapter.feed(0x0a8, data), 1)
        row = store.snapshot()[0]
        self.assertEqual((row["signal"], row["value"], row["quality"]),
                         ("engine.rpm", 800.0, "candidate"))
        now[0] = .25
        self.assertEqual(store.snapshot()[0]["quality"], "stale")
        self.assertEqual(adapter.feed(0x0a8, b"\x00"), 1)
        self.assertEqual(store.snapshot()[0]["quality"], "decode_error")

    @unittest.skipUnless(importlib.util.find_spec("cantools"), "optional cantools not installed")
    def test_real_cantools_dbc_decode(self):
        database = load_cantools_database(ROOT / "tests/fixtures/minimal.dbc")
        values = database.get_message_by_frame_id(0x0a8).decode(
            bytes.fromhex("0000800c00000000"), decode_choices=False)
        self.assertEqual(values["MO_Drehzahl_01"], 800.0)

    def test_mcd_simple_subset_and_rejection_report(self):
        report = import_mcd_mwb(ROOT / "tests/fixtures/mcd-simple.json", ecu="fixture",
            applicability={"asam_id": "EV_FIXTURE", "asam_version": "001001"})
        self.assertEqual((len(report.definitions), len(report.unsupported)), (1, 1))
        self.assertIn("fixed leaf", report.unsupported[0]["reason"])
        registry = SignalRegistry(report.definitions)
        decoded = registry.decode_uds("fixture", 0x1234, bytes.fromhex("01f4"),
            {"asam_id": "EV_FIXTURE", "asam_version": "001001"})
        self.assertEqual((decoded["value"], decoded["unit"], decoded["quality"]),
                         (10.0, "degC", "candidate"))
        invalid = registry.decode_uds("fixture", 0x1234, bytes.fromhex("ffff"),
            {"asam_id": "EV_FIXTURE", "asam_version": "001001"})
        self.assertEqual(invalid["quality"], "invalid")

    def test_mcd_float_and_texttable_decode_rejects_unknown_values(self):
        report = import_mcd_mwb(ROOT / "tests/fixtures/mcd-typed-scalars.json",
                                ecu="fixture", provider_id="fixture")
        self.assertEqual((len(report.definitions), len(report.unsupported)), (3, 2))
        self.assertIn("overlapping TEXTTABLE", report.unsupported[0]["reason"])
        self.assertIn("unsupported A_FLOAT32 encoding", report.unsupported[1]["reason"])
        self.assertTrue(all(not item.provenance.vehicle_tested for item in report.definitions))
        registry = SignalRegistry(report.definitions)
        big_float = registry.decode_uds("fixture", 0x2201, struct.pack(">f", 12.5), {})
        self.assertEqual((big_float["value"], big_float["unit"], big_float["quality"]),
                         (12.5, "bar", "candidate"))
        little_float = registry.decode_uds("fixture", 0x2202, struct.pack("<f", 4.25), {})
        self.assertEqual(little_float["value"], 8.5)
        self.assertEqual(registry.decode_uds("fixture", 0x2201,
            struct.pack(">f", float("nan")), {})["quality"], "invalid")
        gear = registry.decode_uds("fixture", 0x2203, b"\x02", {})
        self.assertEqual((gear["value"], gear["coded_value"], gear["quality"]),
                         ("GEAR_1", 2, "candidate"))
        self.assertEqual(registry.decode_uds("fixture", 0x2203, b"\x09", {})["quality"],
                         "invalid")
        self.assertEqual(registry.decode_uds("fixture", 0x2203, b"\xff", {})["quality"],
                         "invalid")

    def test_bad_registry_schema(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            path.write_text(json.dumps({"schema_version": 99, "definitions": []}))
            with self.assertRaises(ValueError):
                load_registry(path)


if __name__ == "__main__":
    unittest.main()
