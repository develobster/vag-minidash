"""VAG knowledge provider; VAG/MQB details do not leak into registry core."""
import json
from pathlib import Path

from .knowledge import ProviderMetadata
from .registry import (
    Provenance,
    SignalDefinition,
    SignalSource,
    ValidationEvidence,
    load_registry,
)

CANONICAL_ALIASES = {
    "engine.coolant_temp": "engine.coolant_temperature",
    "engine.oil_temp": "engine.oil_temperature",
    "vehicle.wheel_speed.fl": "wheel.front_left.speed",
    "vehicle.wheel_speed.fr": "wheel.front_right.speed",
    "vehicle.wheel_speed.rl": "wheel.rear_left.speed",
    "vehicle.wheel_speed.rr": "wheel.rear_right.speed",
    "awd.module_temp": "awd.module_temperature",
    "awd.cooling_fin_temp": "awd.cooling_fin_temperature",
    "awd.clutch_temp": "awd.clutch_temperature",
}


class VagKnowledgeProvider:
    def __init__(self, data_paths=None, profile_paths=None):
        root = Path(__file__).resolve().parents[1]
        self.root = root
        self.data_paths = tuple(Path(path) for path in (data_paths or (
            root / "data/external/opendbc-vw-mqb-map.json",
            root / "data/external/community-haldex-gen5.json",
        )))
        self.profile_paths = tuple(Path(path) for path in (profile_paths or
            sorted((root / "data/ecu_profiles").glob("*.json"))))

    @property
    def metadata(self):
        return ProviderMetadata("vag", manufacturer="Volkswagen Group",
            platforms=("MQB",),
            description="First MiniDash knowledge provider; MQB is its first dataset.")

    def definitions(self):
        for path in self.data_paths:
            yield from load_registry(path).definitions
        for path in self.profile_paths:
            profile = json.loads(path.read_text(encoding="utf-8"))
            try:
                profile_source = path.relative_to(self.root).as_posix()
            except ValueError:
                profile_source = f"external/{path.name}"
            fingerprint = {key: value for key, value in profile.get("fingerprint", {}).items()
                           if key in {"sw_part", "sw_version", "hw_part", "hw_version",
                                      "asam_id", "asam_version"}}
            for signal in profile.get("signals", ()):
                status = signal.get("validation_status", "CANDIDATE")
                confidence = "confirmed" if status.startswith("CONFIRMED") else "candidate"
                decoder = signal.get("raw", {}).get("decoder")
                validation_evidence = tuple(
                    ValidationEvidence(
                        scope=item.get("scope", ""),
                        method=item.get("method", ""),
                        result=item.get("result", ""),
                        identity=item.get("identity"),
                        date=item.get("date"),
                        notes=item.get("notes", ""),
                    )
                    for item in signal.get("validation_evidence", ())
                )
                yield SignalDefinition(
                    id=f"minidash.{profile['id']}.{signal['did'].lower()}",
                    canonical_name=CANONICAL_ALIASES.get(
                        signal["canonical_name"], signal["canonical_name"]),
                    ecu=profile["ecu_alias"],
                    source=SignalSource("uds", did=int(signal["did"], 16)),
                    provenance=Provenance(
                        origin=signal.get("provenance_origin", "minidash_test"),
                        confidence=confidence,
                        source_url=f"minidash://{profile_source}",
                        vehicle_tested=(status == "CONFIRMED_LIVE" and
                                        not validation_evidence),
                        verification_scope=signal.get(
                            "verification_scope", "did_association"),
                        notes=signal.get("notes", ""),
                        validation_evidence=validation_evidence),
                    provider_id="vag", applicability=fingerprint,
                    decoder=decoder, unit=signal.get("unit"), ttl=1.0,
                    metadata={"raw_decoder_available": decoder is not None,
                              "validation_status": status,
                              "logical_ecu": profile.get("logical_ecu")})
