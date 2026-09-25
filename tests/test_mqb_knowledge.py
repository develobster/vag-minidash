import importlib.util
import json
import unittest
from pathlib import Path

from minidash_live.dbc import DbcAdapter, load_cantools_database
from minidash_live.identity import EcuIdentity, IdentityContext, VehicleIdentity
from minidash_live.pipeline import AcquisitionPlanner, SourceAvailability, signal_catalog
from minidash_live.registry import SignalRegistry, load_registry
from minidash_live.runtime import SignalStore
from minidash_live.vag import VagKnowledgeProvider


ROOT = Path(__file__).resolve().parents[1]


def tiguan_context():
    return IdentityContext(
        VehicleIdentity(manufacturer="Volkswagen", platform="MQB",
                        model="Tiguan II / MQB", model_year=2017),
        EcuIdentity(logical_role="engine", part_number="5NA907115D",
                    software_version="0003", hardware_part_number="06L907309B",
                    hardware_version="H31", asam_id="EV_ECM20TFS0115NA907115D",
                    asam_version="001004", ecu_family="CZPA"),
    )


class MqbKnowledgeTests(unittest.TestCase):
    def setUp(self):
        self.provider = VagKnowledgeProvider()
        self.registry = SignalRegistry(self.provider.definitions())

    def test_provider_loads_normalized_candidates_with_licensing(self):
        by_id = {item.id: item for item in self.registry.definitions}
        self.assertGreaterEqual(len(by_id), 80)
        for identifier in (
            "opendbc.vw_mqb.engine.rpm",
            "opendbc.vw_mqb.engine.throttle",
            "opendbc.vw_mqb.engine.relative_manifold_pressure",
            "opendbc.vw_mqb.transmission.target_gear",
            "opendbc.vw_mqb.brake.pressure",
            "opendbc.vw_mqb.steering.angle_magnitude",
            "opendbc.vw_mqb.vehicle.longitudinal_acceleration",
            "opendbc.vw_mqb.vehicle.turn_signal.left",
            "community.octavia_a5.cluster.221b",
        ):
            self.assertIn(identifier, by_id)
        opendbc = by_id["opendbc.vw_mqb.engine.rpm"]
        self.assertEqual(opendbc.provenance.license, "MIT")
        self.assertEqual(opendbc.provenance.source_revision,
                         "c4465696dc2ef0d1340ae188810ac982586999a4")
        self.assertEqual(opendbc.metadata["applicability_status"], "external_unverified")
        self.assertIsNone(opendbc.expected_period)
        self.assertFalse(opendbc.provenance.vehicle_tested)

    def test_octavia_dids_do_not_resolve_for_tiguan(self):
        self.assertEqual(self.registry.resolve_uds("cluster", 0xF40C, tiguan_context()), ())
        exact_tiguan_oil = self.registry.require_uds("engine", 0x11BD, tiguan_context())
        self.assertIn("j623-czpa", exact_tiguan_oil.id)
        octavia = IdentityContext(
            VehicleIdentity(manufacturer="Skoda", model="Octavia A5"),
            EcuIdentity(logical_role="cluster", request_id="714", response_id="77E"),
        )
        candidate = self.registry.require_uds("cluster", 0xF40C, octavia)
        self.assertEqual(candidate.provenance.confidence, "candidate")
        self.assertIsNone(candidate.decoder)
        self.assertEqual(candidate.metadata["reported_request_period_ms"], 10)
        self.assertEqual(self.registry.resolve_uds("cluster", 0x11BD, octavia), ())

    def test_candidate_layouts_match_pinned_frame_ids_and_bit_scaling(self):
        by_id = {item.id: item for item in self.registry.definitions}
        rpm = by_id["opendbc.vw_mqb.engine.rpm"]
        throttle = by_id["opendbc.vw_mqb.engine.throttle"]
        pressure = by_id["opendbc.vw_mqb.engine.relative_manifold_pressure"]
        wheel = by_id["opendbc.vw_mqb.wheel.fl"]
        self.assertEqual((rpm.metadata["layout"]["can_id"],
                          rpm.metadata["layout"]["factor"]), (168, .25))
        self.assertEqual((throttle.metadata["layout"]["can_id"],
                          throttle.metadata["layout"]["start_bit"],
                          throttle.metadata["layout"]["factor"]), (289, 12, .4))
        self.assertEqual((pressure.metadata["layout"]["start_bit"],
                          pressure.metadata["layout"]["length"],
                          pressure.metadata["layout"]["factor"]), (30, 6, 18))
        self.assertEqual((wheel.metadata["layout"]["can_id"],
                          wheel.metadata["layout"]["factor"]), (178, .0075))

    def test_tiguan_catalog_exposes_source_license_and_unverified_scope(self):
        rows = signal_catalog(self.registry, tiguan_context().vehicle,
                              {"engine": tiguan_context().ecu})
        rpm = next(row for row in rows
                   if row["definition_id"] == "opendbc.vw_mqb.engine.rpm")
        self.assertIn("c4465696", rpm["source_url"])
        self.assertEqual(rpm["source_license"], "MIT")
        self.assertEqual(rpm["confidence"], "candidate")
        self.assertFalse(rpm["validated_for_identity"])
        self.assertNotIn("community.octavia_a5.cluster.f40c",
                         {row["definition_id"] for row in rows})

    def test_planner_waits_for_passive_can_then_falls_back_to_uds(self):
        now = [0.0]
        availability = SourceAvailability(self.registry.definitions, clock=lambda: now[0])
        context = tiguan_context()
        planner = AcquisitionPlanner(self.registry, context.vehicle,
            {"engine": context.ecu}, availability,
            poll_periods={"engine.rpm": 1.0}, passive_grace=0.75)
        plan = planner.build(("engine.rpm",))
        can_definition = next(item for item in plan.definitions if item.source.kind == "can")
        job = next(item for item in plan.jobs if item.signal == "engine.rpm")
        self.assertFalse(job.enabled)
        planner.reconcile(plan, [])
        self.assertFalse(job.enabled)
        planner.reconcile(plan, [{"signal":"engine.rpm", "source":"can",
                                  "value":900, "quality":"candidate"}])
        self.assertFalse(job.enabled)
        now[0] = 0.76
        planner.reconcile(plan, [])
        self.assertTrue(job.enabled)
        planner.reconcile(plan, [{"signal":"engine.rpm", "source":"can",
                                  "value":900, "quality":"candidate"}])
        self.assertFalse(job.enabled)
        self.assertEqual(can_definition.provenance.confidence, "candidate")
        with self.assertRaises(ValueError):
            AcquisitionPlanner(self.registry, context.vehicle,
                {"engine": context.ecu}, availability, passive_grace=float("nan"))

    @unittest.skipUnless(importlib.util.find_spec("cantools"), "optional cantools not installed")
    def test_pinned_layout_fixture_decodes_as_candidate_not_confirmed(self):
        registry = load_registry(ROOT / "data/external/opendbc-vw-mqb-candidates.json")
        # Merge the original nine definitions used by the DBC adapter.
        full = SignalRegistry([
            *load_registry(ROOT / "data/external/opendbc-vw-mqb-map.json").definitions,
            *registry.definitions,
        ])
        database = load_cantools_database(ROOT / "tests/fixtures/mqb-candidate-subset.dbc")
        store = SignalStore(lambda: 0.0)
        context = tiguan_context()
        adapter = DbcAdapter(database, "opendbc/vw_mqb", full, store,
                             context=context)
        msg = database.get_message_by_frame_id(0x0FD)
        data = msg.encode({"ESP_v_Signal": 30.0})
        self.assertEqual(adapter.feed(0x0FD, data), 1)
        row = store.snapshot()[0]
        self.assertEqual(row["signal"], "vehicle.speed")
        self.assertAlmostEqual(row["value"], 30.0)
        self.assertEqual(row["quality"], "candidate")

    def test_candidate_files_are_valid_json_and_have_no_raw_period_claims(self):
        for path in ("opendbc-vw-mqb-map.json", "opendbc-vw-mqb-candidates.json",
                     "community-vag-uds-candidates.json", "community-haldex-gen5.json"):
            data = json.loads((ROOT / "data/external" / path).read_text(encoding="utf-8"))
            self.assertEqual(data["schema_version"], 1)
            for definition in data["definitions"]:
                if definition["provenance"]["origin"] in ("opendbc", "habr-project"):
                    self.assertFalse(definition["provenance"]["vehicle_tested"])
                    self.assertIsNone(definition.get("expected_period"))
                if definition["provenance"]["origin"] == "opendbc":
                    layout = definition["metadata"]["layout"]
                    self.assertIsInstance(layout, dict)
                    self.assertIn("can_id", layout)
                    self.assertIsNone(definition["metadata"]["expected_frequency_hz"])
        manifest = json.loads((ROOT / "docs/runtime/tiguan-mqb-validation-manifest.json")
                              .read_text(encoding="utf-8"))
        self.assertIn("0x714", manifest["safety"]["community_octavia_addresses_must_not_be_used"])
        self.assertIsNone(manifest["expected_period_hz"])


if __name__ == "__main__":
    unittest.main()
