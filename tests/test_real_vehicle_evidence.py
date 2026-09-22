import json
import unittest
from pathlib import Path

from minidash_live.discovery import (
    DEFAULT_IDENTITY_PROBES,
    DiscoverySnapshot,
    discover_endpoint,
    verify_discovered_identity,
)
from minidash_live.identity import EcuIdentity, IdentityContext, VehicleIdentity
from minidash_live.knowledge import registry_from_providers
from minidash_live.pipeline import AcquisitionPlanner, SourceAvailability
from minidash_live.uds import ProtocolError, ReadClient
from minidash_live.vag import VagKnowledgeProvider

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / "data/ecu_profiles/j623-czpa-5na907115d.json"


class PduTransport:
    def __init__(self, *responses):
        self.responses = list(responses)
        self.sent = []

    def send(self, pdu, timeout):
        self.sent.append(bytes(pdu))

    def recv(self, timeout):
        return self.responses.pop(0) if self.responses else None

    def close(self):
        pass


class RealVehicleEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.profile = json.loads(PROFILE.read_text(encoding="utf-8"))
        cls.registry = registry_from_providers([
            VagKnowledgeProvider(data_paths=(), profile_paths=(PROFILE,))
        ])
        cls.exact_ecu = EcuIdentity(
            logical_role="engine",
            request_id="7E0",
            response_id="7E8",
            part_number="5NA907115D",
            software_version="0003",
            hardware_part_number="06L907309B",
            hardware_version="H31",
            asam_id="EV_ECM20TFS0115NA907115D",
            asam_version="001004",
        )
        cls.exact = IdentityContext(VehicleIdentity(), cls.exact_ecu)

    def test_transport_evidence_is_structured_and_sanitized(self):
        route = self.profile["addressing"]
        self.assertEqual(route["can"], {
            "request_id": "7E0",
            "response_id": "7E8",
            "bitrate": 500000,
            "identifier_bits": 11,
            "isotp_addressing": "normal",
            "protocol": "UDS on ISO-TP on classic CAN",
            "logical_link": "LL_EnginContrModul1UDS",
        })
        self.assertEqual(route["evidence"]["classification"],
                         "confirmed_real_vehicle")
        self.assertNotIn("C:\\", json.dumps(route))

    def test_f405_is_exact_identity_scoped(self):
        definition = self.registry.require_uds("engine", 0xF405, self.exact)
        self.assertEqual(definition.canonical_name, "engine.coolant_temperature")
        self.assertEqual(definition.provenance.origin, "real_vehicle_validation")
        self.assertTrue(definition.provenance.validated("raw_decode", self.exact))

        generic_mqb = IdentityContext(
            VehicleIdentity(manufacturer="VAG", platform="MQB"),
            EcuIdentity(logical_role="engine", request_id="7E0", response_id="7E8"),
        )
        unrelated = IdentityContext(
            VehicleIdentity(),
            EcuIdentity(logical_role="engine", part_number="OTHER",
                        software_version="0003", asam_id=self.exact_ecu.asam_id,
                        asam_version=self.exact_ecu.asam_version),
        )
        self.assertEqual(self.registry.resolve_uds("engine", 0xF405, generic_mqb), ())
        self.assertEqual(self.registry.resolve_uds("engine", 0xF405, unrelated), ())

    def test_f405_codec_and_malformed_payload(self):
        for raw, expected in ((0x32, 10), (0x44, 28), (0x45, 29)):
            with self.subTest(raw=raw):
                reading = self.registry.decode_uds(
                    "engine", 0xF405, bytes([raw]), self.exact)
                self.assertEqual(reading["value"], expected)
                self.assertEqual(reading["quality"], "valid")
                self.assertEqual(reading["unit"], "degC")
        for payload in (b"", b"\x45\x00"):
            with self.subTest(payload=payload):
                reading = self.registry.decode_uds(
                    "engine", 0xF405, payload, self.exact)
                self.assertIsNone(reading["value"])
                self.assertEqual(reading["quality"], "invalid_length")
        self.assertEqual(self.registry.resolve_uds("engine", 0xF406, self.exact), ())

    def test_f405_uds_response_did_mismatch_is_rejected(self):
        client = ReadClient(PduTransport(bytes.fromhex("62f40645")),
                            interval=.001)
        with self.assertRaises(ProtocolError):
            client.read_did(0xF405)

    def test_endpoint_or_f190_alone_cannot_authorize_f405(self):
        transport = PduTransport(b"\x62\xf1\x90TESTVIN0000000001")
        vehicle, discovered = discover_endpoint(
            "engine", lambda: transport,
            identity=self.exact_ecu,
            probes=DEFAULT_IDENTITY_PROBES,
            client_options={"interval": .001},
        )
        self.assertEqual(transport.sent, [b"\x22\xf1\x90"])
        self.assertIsNone(discovered.identity.part_number)
        self.assertIsNone(discovered.identity.asam_id)
        context = IdentityContext(vehicle, discovered.identity)
        self.assertEqual(self.registry.resolve_uds("engine", 0xF405, context), ())
        self.assertFalse(verify_discovered_identity(
            PduTransport(b"\x62\xf1\x90TESTVIN0000000001"),
            discovered, DEFAULT_IDENTITY_PROBES))

        planner = AcquisitionPlanner(
            self.registry, vehicle, {"engine": discovered.identity},
            SourceAvailability(self.registry.definitions),
            poll_periods={"engine.coolant_temperature": 2.0},
        )
        plan = planner.build(("engine.coolant_temperature",))
        self.assertEqual(plan.jobs, [])
        self.assertEqual(plan.skipped[0]["reason"], "no_applicable_definition")

    def test_exact_observed_identity_makes_conservative_poll_job_eligible(self):
        planner = AcquisitionPlanner(
            self.registry, VehicleIdentity(), {"engine": self.exact_ecu},
            SourceAvailability(self.registry.definitions),
            poll_periods={"engine.coolant_temperature": 2.0},
        )
        plan = planner.build(("engine.coolant_temperature",))
        self.assertEqual(len(plan.jobs), 1)
        self.assertEqual((plan.jobs[0].did, plan.jobs[0].period), (0xF405, 2.0))
        self.assertTrue(plan.jobs[0].require_verified)

    def test_persisted_snapshot_redacts_vin(self):
        transport = PduTransport(b"\x62\xf1\x90TESTVIN0000000001")
        vehicle, discovered = discover_endpoint(
            "engine", lambda: transport,
            identity=EcuIdentity(logical_role="engine", request_id="7E0",
                                 response_id="7E8"),
            client_options={"interval": .001},
        )
        result = DiscoverySnapshot(vehicle, (discovered,)).as_dict(
            redact_sensitive=True)
        serialized = json.dumps(result)
        self.assertNotIn("TESTVIN0000000001", serialized)
        self.assertIsNone(result["vehicle"]["vin"])
        self.assertTrue(result["ecus"][0]["fields"][0]["redacted"])


if __name__ == "__main__":
    unittest.main()
