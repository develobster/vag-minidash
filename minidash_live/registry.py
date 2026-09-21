"""Data-driven signal definitions shared by active and passive readers.

The registry deliberately stores knowledge, not transport objects.  A definition
is only selected when all of its applicability selectors match the observed
vehicle/ECU context; missing identity data is never treated as a wildcard.
"""
from dataclasses import dataclass, field
from collections.abc import Mapping
import json
import math
from pathlib import Path
import re
from types import MappingProxyType

from .profiles import decode as decode_scalar


CONFIDENCE = {"confirmed", "documented", "candidate", "guessed"}
SOURCE_KINDS = {"can", "uds"}


class DuplicateDefinitionError(ValueError):
    pass


class ConflictingDefinitionError(ValueError):
    pass


class UnsupportedVariant(LookupError):
    pass


def _normal(value):
    return "" if value is None else str(value).strip().upper()


@dataclass(frozen=True)
class Provenance:
    origin: str
    confidence: str
    source_url: str
    vehicle_tested: bool = False
    verification_scope: str = "definition_only"
    notes: str = ""

    def __post_init__(self):
        if not self.origin or self.confidence not in CONFIDENCE or not self.source_url:
            raise ValueError("Complete provenance and a valid confidence are required")
        if type(self.vehicle_tested) is not bool or not self.verification_scope:
            raise ValueError("Invalid provenance verification metadata")


@dataclass(frozen=True)
class SignalSource:
    kind: str
    did: int | None = None
    database: str | None = None
    message: str | None = None
    signal: str | None = None

    def __post_init__(self):
        if self.kind not in SOURCE_KINDS:
            raise ValueError("Unknown signal source")
        if self.kind == "uds":
            if type(self.did) is not int or not 0 <= self.did <= 0xffff:
                raise ValueError("UDS source requires a 16-bit DID")
            if any((self.database, self.message, self.signal)):
                raise ValueError("UDS source contains CAN fields")
        elif not all((self.database, self.message, self.signal)) or self.did is not None:
            raise ValueError("CAN source requires database, message and signal")

    def key(self):
        return (self.kind, self.did, self.database, self.message, self.signal)


@dataclass(frozen=True)
class SignalDefinition:
    id: str
    canonical_name: str
    ecu: str
    source: SignalSource
    provenance: Provenance
    applicability: Mapping = field(default_factory=dict)
    decoder: Mapping | None = None
    unit: str | None = None
    ttl: float = 1.0

    def __post_init__(self):
        if not self.id or not re.fullmatch(r"[a-z0-9][a-z0-9._-]*", self.id):
            raise ValueError("Stable lowercase definition id required")
        if not self.canonical_name or not self.ecu:
            raise ValueError("Canonical name and ECU are required")
        if not isinstance(self.applicability, Mapping) or any(not str(k) for k in self.applicability):
            raise ValueError("Applicability must be an object")
        if isinstance(self.ttl, bool) or not isinstance(self.ttl, (int, float)) \
                or not math.isfinite(self.ttl) or self.ttl <= 0:
            raise ValueError("Positive finite TTL required")
        if self.source.kind == "uds" and self.decoder is not None:
            _validate_decoder(self.decoder)
        object.__setattr__(self, "applicability", MappingProxyType(dict(self.applicability)))
        if self.decoder is not None:
            object.__setattr__(self, "decoder", MappingProxyType(dict(self.decoder)))

    def applies_to(self, context):
        return all(_normal(context.get(key)) == _normal(value)
                   for key, value in self.applicability.items())

    def selector_key(self):
        selector = tuple(sorted((str(k), _normal(v)) for k, v in self.applicability.items()))
        return (self.ecu, self.source.key(), selector)


def _validate_decoder(codec):
    required = {"length", "byteorder", "signed", "scale", "offset", "source"}
    if not isinstance(codec, Mapping) or not required <= codec.keys():
        raise ValueError("Incomplete scalar decoder")
    if type(codec["length"]) is not int or not 1 <= codec["length"] <= 8:
        raise ValueError("Invalid decoder length")
    if codec["byteorder"] not in ("big", "little") or type(codec["signed"]) is not bool:
        raise ValueError("Invalid decoder representation")
    if not codec["source"] or not all(isinstance(codec[x], (int, float)) and
            not isinstance(codec[x], bool) and math.isfinite(codec[x]) for x in ("scale", "offset")):
        raise ValueError("Invalid decoder scaling/evidence")


class SignalRegistry:
    def __init__(self, definitions=()):
        self._by_id = {}
        self._by_selector = {}
        for definition in definitions:
            self.add(definition)

    @property
    def definitions(self):
        return tuple(self._by_id.values())

    def add(self, definition):
        if definition.id in self._by_id:
            raise DuplicateDefinitionError(definition.id)
        key = definition.selector_key()
        if key in self._by_selector:
            raise ConflictingDefinitionError(
                f"{definition.id} conflicts with {self._by_selector[key].id}")
        self._by_id[definition.id] = definition
        self._by_selector[key] = definition

    def _resolve(self, predicate, context):
        found = [item for item in self.definitions if predicate(item) and item.applies_to(context)]
        if not found:
            return ()
        specificity = max(len(item.applicability) for item in found)
        best = tuple(item for item in found if len(item.applicability) == specificity)
        if len(best) > 1:
            raise ConflictingDefinitionError("Ambiguous equally-specific definitions")
        return best

    def resolve_uds(self, ecu, did, context):
        return self._resolve(lambda item: item.ecu == ecu and item.source.kind == "uds" and
                             item.source.did == did, context)

    def require_uds(self, ecu, did, context):
        found = self.resolve_uds(ecu, did, context)
        if not found:
            raise UnsupportedVariant(f"No definition for {ecu} DID 0x{did:04X} and this identity")
        return found[0]

    def can_for_message(self, database, message, context):
        found = [item for item in self.definitions if item.source.kind == "can" and
                 item.source.database == database and item.source.message == message and
                 item.applies_to(context)]
        selected = []
        for signal in dict.fromkeys(item.source.signal for item in found):
            choices = [item for item in found if item.source.signal == signal]
            specificity = max(len(item.applicability) for item in choices)
            best = [item for item in choices if len(item.applicability) == specificity]
            if len(best) != 1:
                raise ConflictingDefinitionError(
                    f"Ambiguous equally-specific CAN definitions for {message}/{signal}")
            selected.append(best[0])
        return tuple(selected)

    def decode_uds(self, ecu, did, payload, context, *, identity_verified=True):
        found = self.resolve_uds(ecu, did, context)
        if not found:
            return {"did": f"{did:04X}", "signal": f"unknown.did.{did:04X}",
                    "canonical_name": f"unknown.did.{did:04X}", "unit": None,
                    "raw": payload.hex(), "value": None, "quality": "raw_only",
                    "validation_status": "CANDIDATE"}
        item = found[0]
        codec = dict(item.decoder) if item.decoder else None
        if codec is not None:
            verified_codec = (item.provenance.confidence == "confirmed" and
                              item.provenance.vehicle_tested and
                              item.provenance.verification_scope == "raw_decode")
            codec["status"] = "confirmed" if verified_codec else "candidate"
        legacy = {"did": f"{did:04X}", "canonical_name": item.canonical_name,
                  "unit": item.unit, "validation_status": "CONFIRMED_LIVE"
                  if item.provenance.vehicle_tested else "CANDIDATE",
                  "raw": {"decoder": codec}}
        return decode_scalar(legacy, payload, identity_verified=identity_verified)


def _parse_source(data):
    kind = data.get("kind")
    did = data.get("did")
    if isinstance(did, str):
        did = int(did, 16)
    return SignalSource(kind=kind, did=did, database=data.get("database"),
                        message=data.get("message"), signal=data.get("signal"))


def definition_from_dict(data):
    provenance = data.get("provenance", {})
    return SignalDefinition(
        id=data.get("id", ""), canonical_name=data.get("canonical_name", ""),
        ecu=data.get("ecu", ""), source=_parse_source(data.get("source", {})),
        provenance=Provenance(**provenance), applicability=dict(data.get("applicability", {})),
        decoder=data.get("decoder"), unit=data.get("unit"), ttl=data.get("ttl", 1.0))


def load_registry(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or not isinstance(data.get("definitions"), list):
        raise ValueError("Unsupported signal registry schema")
    return SignalRegistry(definition_from_dict(item) for item in data["definitions"])
