import unittest
from pathlib import Path

from minidash_live.arbitration import AmbiguousSourceError, SourceArbitrator
from minidash_live.identity import EcuIdentity, IdentityContext, VehicleIdentity
from minidash_live.knowledge import ProviderMetadata, registry_from_providers
from minidash_live.mcd_import import import_mcd_mwb
from minidash_live.registry import (
    ConflictingDefinitionError, Provenance, SignalDefinition, SignalRegistry,
    SignalSource, ValidationEvidence, load_registry,
)
from minidash_live.runtime import SignalStore
from minidash_live.vag import VagKnowledgeProvider


ROOT = Path(__file__).resolve().parents[1]
PROVENANCE = Provenance("fixture", "documented", "fixture://knowledge")


def definition(identifier, applicability, *, kind="uds", did=0x1234,
               canonical="engine.rpm", provider="test"):
    source = SignalSource(kind, did=did) if kind == "uds" else SignalSource(
        kind, database="fixture", message="Engine", signal="RPM")
    return SignalDefinition(identifier, canonical, "engine", source, PROVENANCE,
                            provider_id=provider, applicability=applicability)


class SyntheticProvider:
    metadata = ProviderMetadata("test", manufacturer="TEST", platforms=("P1",))
    def definitions(self):
        return (definition("test.synthetic.rpm", {"manufacturer": "TEST"}),)


class VehicleKnowledgeTests(unittest.TestCase):
    def full_identity(self):
        return IdentityContext(
            VehicleIdentity(manufacturer="TEST", platform="P1", model="Model", model_year=2020),
            EcuIdentity(logical_role="engine", part_number="PN", software_version="1",
                        hardware_part_number="HW", asam_id="EV_X", asam_version="001",
                        ecu_family="FAM"))

    def test_semantic_specificity_hierarchy(self):
        definitions = [
            definition("test.generic", {}),
            definition("test.manufacturer", {"manufacturer": "TEST"}),
            definition("test.platform", {"platform": "P1"}),
            definition("test.family", {"platform": "P1", "ecu_family": "FAM"}),
            definition("test.asam", {"asam_id": "EV_X"}),
            definition("test.software", {"asam_id": "EV_X", "asam_version": "001"}),
            definition("test.exact", {"part_number": "PN", "software_version": "1",
                                      "hardware_part_number": "HW", "asam_id": "EV_X"}),
        ]
        selected = SignalRegistry(definitions).require_uds("engine", 0x1234, self.full_identity())
        self.assertEqual(selected.id, "test.exact")
        scopes = [item.applicability.specificity().scope for item in definitions]
        self.assertEqual(scopes, sorted(scopes))

    def test_asam_software_beats_platform_family_with_equal_field_count(self):
        family = definition("test.family", {"platform": "P1", "ecu_family": "FAM"})
        variant = definition("test.variant", {"asam_id": "EV_X", "software_version": "1"})
        selected = SignalRegistry([family, variant]).require_uds(
            "engine", 0x1234, self.full_identity())
        self.assertEqual(selected.id, "test.variant")

    def test_missing_identity_is_not_wildcard(self):
        registry = SignalRegistry([definition("test.platform", {"platform": "P1"})])
        self.assertEqual(registry.resolve_uds("engine", 0x1234,
                                             IdentityContext(VehicleIdentity(manufacturer="TEST"))), ())

    def test_ambiguous_equal_semantics_fails_safely(self):
        by_model = definition("test.model", {"model": "Model"})
        by_year = definition("test.year", {"model_year": 2020})
        with self.assertRaises(ConflictingDefinitionError):
            SignalRegistry([by_model, by_year]).require_uds(
                "engine", 0x1234, self.full_identity())

    def test_exact_duplicate_selector_conflicts_at_load(self):
        first = definition("test.one", {"platform": "P1"})
        second = definition("test.two", {"platform": "P1"})
        with self.assertRaises(ConflictingDefinitionError):
            SignalRegistry([first, second])

    def test_multiple_can_and_uds_implementations(self):
        identity = self.full_identity()
        definitions = [
            definition("test.can.rpm", {"platform": "P1"}, kind="can"),
            definition("test.uds.rpm", {"ecu_family": "FAM"}),
        ]
        implementations = SignalRegistry(definitions).implementations("engine.rpm", identity)
        self.assertEqual({item.source.kind for item in implementations}, {"can", "uds"})

    def test_arbitration_quality_then_evidence_then_cost(self):
        now = [0.0]
        store = SignalStore(lambda: now[0])
        store.publish("engine", "engine.rpm", "can", 800, 1, "candidate", "rpm",
                      definition_id="can", confidence="documented", active_cost=0)
        store.publish("engine", "engine.rpm", "uds", 801, 1, "valid", "rpm",
                      definition_id="uds", confidence="confirmed", active_cost=1)
        self.assertEqual(store.best("engine.rpm", SourceArbitrator())["definition_id"], "uds")
        store.publish("engine", "engine.rpm", "can", 802, 1, "valid", "rpm",
                      definition_id="can", confidence="confirmed", active_cost=0)
        self.assertEqual(store.best("engine.rpm", SourceArbitrator())["definition_id"], "can")

    def test_equal_source_scores_are_ambiguous(self):
        store = SignalStore(lambda: 0.0)
        for identifier, source in (("a", "can"), ("b", "uds")):
            store.publish("engine", "engine.rpm", source, 800, 1, "valid", "rpm",
                          definition_id=identifier, confidence="confirmed", active_cost=0)
        with self.assertRaises(AmbiguousSourceError):
            store.best("engine.rpm", SourceArbitrator())

    def test_synthetic_non_vag_provider(self):
        registry = registry_from_providers([SyntheticProvider()])
        selected = registry.require_uds("engine", 0x1234,
            IdentityContext(VehicleIdentity(manufacturer="TEST")))
        self.assertEqual((selected.provider_id, selected.canonical_name), ("test", "engine.rpm"))

    def test_vag_provider_is_data_layer(self):
        provider = VagKnowledgeProvider()
        registry = registry_from_providers([provider])
        self.assertTrue(registry.definitions)
        self.assertTrue(all(item.provider_id == "vag" for item in registry.definitions))

    def test_multi_variant_isolation(self):
        registry = load_registry(ROOT / "tests/fixtures/knowledge-variants.json")
        dq500 = IdentityContext(VehicleIdentity(manufacturer="VAG", platform="MQB"),
            EcuIdentity(ecu_family="DQ500", part_number="0DL300011P",
                        software_version="2004", asam_id="EV_TCMDQ500021",
                        asam_version="001001"))
        selected = registry.require_uds("transmission", 0x3816, dq500)
        self.assertEqual(selected.id, "fixture.vag.dq500.0dl300011p.gear")
        unknown = IdentityContext(VehicleIdentity(manufacturer="VAG", platform="MQB"),
                                  EcuIdentity(ecu_family="UNKNOWN"))
        self.assertEqual(registry.resolve_uds("transmission", 0x3816, unknown), ())
        ea211 = IdentityContext(VehicleIdentity(manufacturer="VAG", platform="MQB"),
                                EcuIdentity(ecu_family="EA211"))
        implementations = registry.implementations("engine.coolant_temperature", ea211)
        self.assertEqual([item.id for item in implementations], ["fixture.vag.ea211.coolant"])

    def test_mcd_envelope_preserves_variant_and_raw_namespace(self):
        report = import_mcd_mwb(ROOT / "tests/fixtures/mcd-envelope.json", ecu="engine")
        self.assertEqual(len(report.definitions), 1)
        item = report.definitions[0]
        self.assertEqual(item.canonical_name, "raw.vag.engine.2222.engine.speed")
        self.assertEqual(item.metadata["mcd_base_variant"], "BV_TestEngineUDS")
        identity = IdentityContext(VehicleIdentity(manufacturer="Volkswagen", platform="MQB"),
            EcuIdentity(ecu_family="TEST_ENGINE", asam_id="EV_TEST_ENGINE", asam_version="001"))
        self.assertTrue(item.applies_to(identity))

    def test_mcd_canonical_mapping_is_explicit(self):
        report = import_mcd_mwb(ROOT / "tests/fixtures/mcd-envelope.json", ecu="engine",
                                canonical_map={0x2222: "engine.rpm"})
        self.assertEqual(report.definitions[0].canonical_name, "engine.rpm")

    def test_validation_evidence_is_identity_scoped(self):
        tested = self.full_identity()
        evidence = ValidationEvidence("raw_decode", "bench comparison", "pass", tested,
                                      date="2026-09-21")
        provenance = Provenance("fixture", "confirmed", "fixture://evidence",
                                validation_evidence=(evidence,))
        self.assertTrue(provenance.validated("raw_decode", tested))
        other = IdentityContext(tested.vehicle, EcuIdentity(part_number="OTHER"))
        self.assertFalse(provenance.validated("raw_decode", other))

    def test_validation_evidence_specificity_and_conflict_fail_closed(self):
        tested = self.full_identity()
        generic_failure = ValidationEvidence("raw_decode", "generic test", "fail")
        exact_pass = ValidationEvidence("raw_decode", "vehicle test", "pass", tested)
        provenance = Provenance("fixture", "confirmed", "fixture://evidence",
                                validation_evidence=(generic_failure, exact_pass))
        self.assertTrue(provenance.validated("raw_decode", tested))
        conflict = Provenance("fixture", "confirmed", "fixture://evidence",
            validation_evidence=(exact_pass,
                ValidationEvidence("raw_decode", "repeat test", "fail", tested)))
        self.assertFalse(conflict.validated("raw_decode", tested))

    def test_nested_identity_json_shape(self):
        identity = IdentityContext.from_mapping({
            "vehicle": {"manufacturer": "TEST", "platform": "P1"},
            "ecu": {"ecu_family": "FAM", "asam_id": "EV_X"},
        })
        self.assertEqual(identity.vehicle.platform, "P1")
        self.assertEqual(identity.ecu.asam_id, "EV_X")


if __name__ == "__main__":
    unittest.main()
