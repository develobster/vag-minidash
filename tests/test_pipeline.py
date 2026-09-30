import importlib.util
import io
import json
import tempfile
import threading
import time
import unittest
import uuid
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

from minidash_live.__main__ import main
from minidash_live.dbc import load_cantools_database
from minidash_live.discovery import (
    IdentityProbe,
    discover_vehicle,
    verify_discovered_identity,
)
from minidash_live.identity import EcuIdentity, VehicleIdentity
from minidash_live.pipeline import (
    AcquisitionPlanner,
    RuntimeEngine,
    SourceAvailability,
    signal_catalog,
)
from minidash_live.registry import (
    Provenance,
    SignalDefinition,
    SignalRegistry,
    SignalSource,
)
from minidash_live.runtime import EcuSession
from minidash_live.transports import CanTransport

ROOT = Path(__file__).resolve().parents[1]
AVAILABLE = all(importlib.util.find_spec(name) for name in ("can", "isotp", "cantools"))


class VirtualEcu:
    def __init__(self, channel, txid=0x708, rxid=0x700):
        import can
        import isotp
        self.bus = can.Bus(interface="virtual", channel=channel, ignore_config=True)
        self.stack = isotp.CanStack(bus=self.bus,
            address=isotp.Address(isotp.AddressingMode.Normal_11bits, txid=txid, rxid=rxid))
        self.stack.start()
        self.stop = threading.Event()
        self.requests = []
        self.errors = []
        self.responses = {
            0xF190: b"TESTVIN0000000001",
            0xF187: b"TEST-PN",
            0xF189: b"0001",
            0xF19E: b"EV_TEST_ENGINE",
            0xF1A0: b"001",
            0x1234: (3204).to_bytes(2, "big"),
            0x1235: bytes([100]),
        }
        self.worker = threading.Thread(target=self._serve, name="minidash-test-ecu")
        self.worker.start()

    def _serve(self):
        try:
            while not self.stop.is_set():
                request = self.stack.recv(block=True, timeout=.02)
                if request is None:
                    continue
                request = bytes(request)
                self.requests.append(request)
                if request == b"\x10\x01":
                    self.stack.send(bytes.fromhex("500100320064"))
                elif request == b"\x3e\x00":
                    self.stack.send(b"\x7e\x00")
                elif len(request) == 3 and request[0] == 0x22:
                    did = int.from_bytes(request[1:], "big")
                    payload = self.responses.get(did)
                    self.stack.send(b"\x7f\x22\x31" if payload is None else
                                    b"\x62" + request[1:] + payload)
        except Exception as exc:
            self.errors.append(exc)

    def close(self):
        self.stop.set()
        self.worker.join(2)
        self.stack.stop()
        self.bus.shutdown()


def fixture_registry(identity):
    provenance = Provenance("virtual_fixture", "confirmed", "fixture://virtual-e2e")
    exact = {"part_number": identity.part_number,
             "software_version": identity.software_version,
             "asam_id": identity.asam_id, "asam_version": identity.asam_version}
    return SignalRegistry([
        SignalDefinition("fixture.can.rpm", "engine.rpm", "engine",
            SignalSource("can", database="fixture", message="Motor_12",
                         signal="MO_Drehzahl_01"), provenance,
            provider_id="test", applicability={"platform": "TEST"},
            unit="rpm", ttl=.10, expected_period=.02),
        SignalDefinition("fixture.uds.rpm", "engine.rpm", "engine",
            SignalSource("uds", did=0x1234), provenance, provider_id="test",
            applicability=exact,
            decoder={"length": 2, "byteorder": "big", "signed": False,
                     "scale": .25, "offset": 0, "invalid_raw": [],
                     "source": "fixture://virtual-e2e"},
            unit="rpm", ttl=1, expected_period=.04),
        SignalDefinition("fixture.uds.coolant", "engine.coolant_temperature", "engine",
            SignalSource("uds", did=0x1235), provenance, provider_id="test",
            applicability=exact,
            decoder={"length": 1, "byteorder": "big", "signed": False,
                     "scale": 1, "offset": -40, "invalid_raw": [],
                     "source": "fixture://virtual-e2e"},
            unit="degC", ttl=1, expected_period=.05),
    ])


@unittest.skipUnless(AVAILABLE, "VirtualBus dependencies required")
class PipelineIntegrationTests(unittest.TestCase):
    def setUp(self):
        import can
        self.channel = "pipeline-" + uuid.uuid4().hex
        self.ecu = VirtualEcu(self.channel)
        self.capture = can.Bus(interface="virtual", channel=self.channel, ignore_config=True)
        self.publisher = can.Bus(interface="virtual", channel=self.channel, ignore_config=True)
        self.probes = (
            IdentityProbe(0xF190, "vehicle.vin"),
            IdentityProbe(0xF187, "ecu.part_number"),
            IdentityProbe(0xF189, "ecu.software_version"),
            IdentityProbe(0xF19E, "ecu.asam_id"),
            IdentityProbe(0xF1A0, "ecu.asam_version"),
        )
        self.options = {"interval": .001, "p2": .2, "p2_star": .2, "total": 1}
        self.factory = lambda: CanTransport(
            self.channel, 0x700, 0x708, 500000, interface="virtual")

    def tearDown(self):
        self.capture.shutdown()
        self.publisher.shutdown()
        self.ecu.close()
        self.assertEqual(self.ecu.errors, [])

    def discover(self):
        return discover_vehicle(VehicleIdentity(manufacturer="TEST", platform="TEST"), [{
            "role": "engine", "factory": self.factory,
            "identity": EcuIdentity(logical_role="engine", request_id="700", response_id="708"),
            "probes": self.probes, "client_options": self.options,
        }])

    def runtime(self):
        snapshot = self.discover()
        discovered = snapshot.ecus[0]
        registry = fixture_registry(discovered.identity)
        availability = SourceAvailability(registry.definitions)
        ecus = {"engine": discovered.identity}
        planner = AcquisitionPlanner(registry, snapshot.vehicle, ecus, availability)
        plan = planner.build(("engine.rpm", "engine.coolant_temperature"))
        def verify(client):
            return verify_discovered_identity(client, discovered, self.probes)
        session = EcuSession(self.factory, verify=verify, client_options=self.options)
        database = load_cantools_database(ROOT / "tests/fixtures/minimal.dbc")
        engine = RuntimeEngine(plan, planner, {"engine": session}, {"fixture": database},
                               cooldown=.001)
        return snapshot, registry, plan, session, engine

    def test_discover_cli_uses_transport_and_writes_snapshot(self):
        config = {
            "vehicle_identity": {"manufacturer": "TEST", "platform": "TEST"},
            "transport": {"kind": "socketcan", "interface": "virtual",
                          "channel": self.channel},
            "uds_timing": self.options,
            "ecus": {"engine": {"txid": "700", "rxid": "708", "bitrate": 500000,
                "address_evidence": "synthetic VirtualBus fixture",
                "identity_probes": [
                    {"did": f"{probe.did:04X}", "target": probe.target,
                     "encoding": probe.encoding} for probe in self.probes]}},
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config_path, output_path = root / "config.json", root / "identity.json"
            config_path.write_text(json.dumps(config), encoding="utf-8")
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                self.assertEqual(main(["discover", "--config", str(config_path),
                                       "--output", str(output_path), "--debug"]), 0)
            snapshot = json.loads(output_path.read_text(encoding="utf-8"))
            self.assertIsNone(snapshot["vehicle"]["vin"])
            vin_field = next(item for item in snapshot["ecus"][0]["fields"]
                             if item["target"] == "vehicle.vin")
            self.assertTrue(vin_field["redacted"])
            self.assertNotIn("value", vin_field)
            self.assertNotIn("TESTVIN0000000001", output_path.read_text(encoding="utf-8"))
            self.assertEqual(snapshot["ecus"][0]["identity"]["asam_id"], "EV_TEST_ENGINE")
            self.assertTrue(snapshot["debug"])

    def test_virtual_bus_end_to_end_and_can_to_uds_fallback(self):
        import can
        snapshot, registry, plan, session, engine = self.runtime()
        try:
            self.assertEqual(snapshot.vehicle.vin, "TESTVIN0000000001")
            self.assertEqual(snapshot.ecus[0].identity.asam_id, "EV_TEST_ENGINE")
            self.assertTrue(snapshot.ecus[0].fingerprint.startswith("ecu-v1:"))
            self.assertEqual(len(plan.passive), 1)
            self.assertEqual({job.did for job in plan.jobs}, {0x1234, 0x1235})

            deadline = time.monotonic() + 2
            while time.monotonic() < deadline:
                engine.step()
                rows = engine.store.snapshot()
                if {row["signal"] for row in rows if row["source"] == "uds" and
                        row["value"] is not None} == {
                            "engine.rpm", "engine.coolant_temperature"}:
                    break
                time.sleep(.002)
            self.assertEqual(engine.store.best("engine.coolant_temperature",
                                               engine.arbitrator)["value"], 60)

            self.publisher.send(can.Message(arbitration_id=0x0A8,
                data=bytes.fromhex("0000800c00000000"), is_extended_id=False))
            capture_deadline = time.monotonic() + 1
            message = None
            while time.monotonic() < capture_deadline:
                candidate = self.capture.recv(.05)
                if candidate is not None and candidate.arbitration_id == 0x0A8:
                    message = candidate
                    break
            self.assertIsNotNone(message)
            engine.feed_message(message)
            engine.step()
            current = engine.snapshot()
            self.assertEqual(current["decisions"]["engine.rpm"]["selected"]["source"], "can")
            self.assertTrue(any(not job.enabled for job in plan.jobs
                                if job.signal == "engine.rpm"))

            time.sleep(.12)
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline:
                engine.step()
                selected = engine.snapshot()["decisions"]["engine.rpm"]["selected"]
                if selected and selected["source"] == "uds":
                    break
                time.sleep(.002)
            result = engine.snapshot()
            self.assertEqual(result["decisions"]["engine.rpm"]["selected"]["source"], "uds")
            self.assertGreaterEqual(result["metrics"]["source_switches"], 1)
            self.assertGreater(result["metrics"]["can_frames_decoded"], 0)
            self.assertGreater(result["metrics"]["uds_responses"], 0)
        finally:
            session.close()

    def test_unknown_ecu_has_no_uds_job(self):
        snapshot = self.discover()
        known = snapshot.ecus[0].identity
        registry = fixture_registry(known)
        unknown = EcuIdentity(logical_role="engine", part_number="OTHER",
                              software_version="9", asam_id="EV_OTHER", asam_version="999")
        availability = SourceAvailability(registry.definitions)
        planner = AcquisitionPlanner(registry, snapshot.vehicle, {"engine": unknown}, availability)
        plan = planner.build(("engine.coolant_temperature",))
        self.assertEqual(plan.jobs, [])
        self.assertEqual(plan.skipped[0]["reason"], "no_applicable_definition")
        catalog = signal_catalog(registry, snapshot.vehicle, {"engine": unknown}, availability)
        self.assertFalse(any(row["canonical_name"] == "engine.coolant_temperature"
                             for row in catalog))

    def test_identity_change_blocks_signal_did(self):
        snapshot = self.discover()
        discovered = snapshot.ecus[0]
        registry = fixture_registry(discovered.identity)
        availability = SourceAvailability(registry.definitions)
        planner = AcquisitionPlanner(registry, snapshot.vehicle,
                                     {"engine": discovered.identity}, availability)
        plan = planner.build(("engine.coolant_temperature",))
        self.ecu.responses[0xF187] = b"OTHER-PN"
        def verify(client):
            return verify_discovered_identity(client, discovered, self.probes)
        session = EcuSession(self.factory, verify=verify, client_options=self.options)
        engine = RuntimeEngine(plan, planner, {"engine": session}, {}, cooldown=.001)
        before = len(self.ecu.requests)
        try:
            engine.step()
            later = self.ecu.requests[before:]
            self.assertNotIn(bytes.fromhex("221235"), later)
            row = engine.store.snapshot()[0]
            self.assertEqual(row["quality"], "identity_unverified")
        finally:
            session.close()


class FingerprintTests(unittest.TestCase):
    def test_watch_duration_starts_after_catalog_setup(self):
        now = [0.0]
        def slow_catalog(*args, **kwargs):
            now[0] += 1.0  # More setup time than the requested acquisition duration.
            return []
        def sleep(seconds):
            now[0] += seconds
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'config.json').write_text('{"ecus":{}}', encoding='utf-8')
            (root / 'identity.json').write_text(json.dumps({
                'schema_version': 1, 'vehicle': {'platform': 'MQB'},
                'ecus': [], 'unavailable': []}), encoding='utf-8')
            output = io.StringIO()
            with redirect_stdout(output), \
                    patch('minidash_live.__main__.signal_catalog', side_effect=slow_catalog), \
                    patch('minidash_live.__main__.time.monotonic', side_effect=lambda: now[0]), \
                    patch('minidash_live.__main__.time.sleep', side_effect=sleep):
                self.assertEqual(main(['watch', '--config', str(root / 'config.json'),
                    '--identity-snapshot', str(root / 'identity.json'),
                    '--duration', '.02', '--interval', '.01', 'engine.rpm']), 0)
            self.assertIn('engine.rpm: unavailable', output.getvalue())

    def test_fingerprint_is_deterministic_and_omits_missing_fields(self):
        first = EcuIdentity(logical_role="engine", asam_id="EV_X", software_version="1")
        second = EcuIdentity(software_version="1", asam_id="EV_X", logical_role="engine")
        self.assertEqual(first.fingerprint(), second.fingerprint())
        self.assertNotIn("part_number", first.fingerprint())
        self.assertEqual(EcuIdentity(request_id=0x700).fingerprint(),
                         EcuIdentity(request_id="0x700").fingerprint())

    def test_signals_and_watch_cli_accept_canonical_names(self):
        snapshot = {"schema_version": 1,
                    "vehicle": {"manufacturer": "VAG", "platform": "MQB",
                                "model": None, "model_year": None, "vin": None},
                    "ecus": [], "unavailable": []}
        config = {"vehicle_identity": {"manufacturer": "VAG", "platform": "MQB"},
                  "ecus": {}}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config_path, snapshot_path = root / "config.json", root / "identity.json"
            config_path.write_text(json.dumps(config), encoding="utf-8")
            snapshot_path.write_text(json.dumps(snapshot), encoding="utf-8")
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(main(["signals", "--config", str(config_path),
                    "--identity-snapshot", str(snapshot_path),
                    "--all-implementations"]), 0)
            self.assertIn("engine.rpm", output.getvalue())
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(main(["watch", "--config", str(config_path),
                    "--identity-snapshot", str(snapshot_path), "--duration", ".2",
                    "--interval", ".01", "engine.rpm"]), 0)
            self.assertIn("engine.rpm: unavailable", output.getvalue())


if __name__ == "__main__":
    unittest.main()
