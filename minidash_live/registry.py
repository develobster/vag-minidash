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

from .identity import Applicability, IdentityContext
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
class ValidationEvidence:
    scope: str
    method: str
    result: str
    identity: IdentityContext | None = None
    date: str | None = None
    notes: str = ""

    def __post_init__(self):
        if not self.scope or not self.method or self.result not in ("pass", "fail"):
            raise ValueError("Validation evidence needs scope, method and pass/fail result")
        if self.identity is not None and not isinstance(self.identity, IdentityContext):
            object.__setattr__(self, "identity", IdentityContext.from_mapping(self.identity))

    def applies_to(self, identity):
        return self.applicability().matches(identity)

    def applicability(self):
        if self.identity is None:
            return Applicability()
        expected = {}
        for group in (self.identity.vehicle, self.identity.ecu):
            for name, value in vars(group).items():
                if value is not None:
                    expected[name] = value
        return Applicability(expected)


@dataclass(frozen=True)
class Provenance:
    origin: str
    confidence: str
    source_url: str
    vehicle_tested: bool = False
    verification_scope: str = "definition_only"
    notes: str = ""
    validation_evidence: tuple = ()
    license: str | None = None
    source_revision: str | None = None

    def __post_init__(self):
        if not self.origin or self.confidence not in CONFIDENCE or not self.source_url:
            raise ValueError("Complete provenance and a valid confidence are required")
        if type(self.vehicle_tested) is not bool or not self.verification_scope:
            raise ValueError("Invalid provenance verification metadata")
        if self.license is not None and not isinstance(self.license, str):
            raise ValueError("Provenance license must be a string")
        if self.source_revision is not None and not isinstance(self.source_revision, str):
            raise ValueError("Provenance source revision must be a string")
        evidence = tuple(item if isinstance(item, ValidationEvidence)
                         else _parse_validation_evidence(item)
                         for item in self.validation_evidence)
        object.__setattr__(self, "validation_evidence", evidence)

    def validated(self, scope, identity):
        matching = [item for item in self.validation_evidence
                    if item.scope == scope and item.applies_to(identity)]
        if matching:
            specificity = max(item.applicability().specificity() for item in matching)
            best = [item for item in matching
                    if item.applicability().specificity() == specificity]
            # Conflicting equally-specific evidence fails closed; array order
            # is never authority.
            return all(item.result == "pass" for item in best)
        # Schema-v1 compatibility. New definitions should use evidence records.
        return self.vehicle_tested and self.verification_scope == scope


@dataclass(frozen=True)
class SignalSource:
    kind: str
    did: int | None = None
    database: str | None = None
    message: str | None = None
    signal: str | None = None
    active_cost: float | None = None

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
        cost = (1.0 if self.kind == "uds" else 0.0) if self.active_cost is None else self.active_cost
        if isinstance(cost, bool) or not isinstance(cost, (int, float)) \
                or not math.isfinite(cost) or cost < 0:
            raise ValueError("Source active cost must be finite and non-negative")
        object.__setattr__(self, "active_cost", float(cost))

    def key(self):
        return (self.kind, self.did, self.database, self.message, self.signal)


@dataclass(frozen=True)
class SignalDefinition:
    id: str
    canonical_name: str
    ecu: str
    source: SignalSource
    provenance: Provenance
    provider_id: str = "generic"
    applicability: Applicability | Mapping = field(default_factory=Applicability)
    decoder: Mapping | None = None
    metadata: Mapping = field(default_factory=dict)
    unit: str | None = None
    ttl: float = 1.0
    expected_period: float | None = None

    def __post_init__(self):
        if not self.id or not re.fullmatch(r"[a-z0-9][a-z0-9._-]*", self.id):
            raise ValueError("Stable lowercase definition id required")
        if not re.fullmatch(r"[a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+", self.canonical_name) or not self.ecu:
            raise ValueError("Canonical name and ECU are required")
        if not re.fullmatch(r"[a-z][a-z0-9_-]*", self.provider_id):
            raise ValueError("Stable lowercase provider id required")
        if isinstance(self.ttl, bool) or not isinstance(self.ttl, (int, float)) \
                or not math.isfinite(self.ttl) or self.ttl <= 0:
            raise ValueError("Positive finite TTL required")
        if self.expected_period is not None and (isinstance(self.expected_period, bool) or
                not isinstance(self.expected_period, (int, float)) or
                not math.isfinite(self.expected_period) or self.expected_period <= 0):
            raise ValueError("Expected period must be positive and finite")
        if self.source.kind == "uds" and self.decoder is not None:
            _validate_decoder(self.decoder)
        if not isinstance(self.metadata, Mapping):
            raise ValueError("Definition metadata must be an object")
        object.__setattr__(self, "applicability", Applicability(self.applicability))
        object.__setattr__(self, "metadata", MappingProxyType(dict(self.metadata)))
        if self.decoder is not None:
            object.__setattr__(self, "decoder", MappingProxyType(dict(self.decoder)))

    def applies_to(self, context):
        return self.applicability.matches(context)

    @property
    def is_raw(self):
        return self.canonical_name.startswith("raw.")

    def selector_key(self):
        return (self.ecu, self.source.key(), self.applicability)


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
        specificity = max(item.applicability.specificity() for item in found)
        best = tuple(item for item in found
                     if item.applicability.specificity() == specificity)
        if len(best) > 1:
            names = ", ".join(item.id for item in best)
            raise ConflictingDefinitionError(
                f"Ambiguous definitions at semantic specificity {specificity.scope}: {names}")
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
                 item.applies_to(context(item) if callable(context) else context)]
        selected = []
        for signal in dict.fromkeys(item.source.signal for item in found):
            choices = [item for item in found if item.source.signal == signal]
            specificity = max(item.applicability.specificity() for item in choices)
            best = [item for item in choices
                    if item.applicability.specificity() == specificity]
            if len(best) != 1:
                raise ConflictingDefinitionError(
                    f"Ambiguous equally-specific CAN definitions for {message}/{signal}")
            selected.append(best[0])
        return tuple(selected)

    def implementations(self, canonical_name, context):
        """Return the best applicable definition for each independent source route."""
        found = [item for item in self.definitions
                 if item.canonical_name == canonical_name and item.applies_to(context)]
        selected = []
        for route in dict.fromkeys((item.ecu, item.source.key()) for item in found):
            choices = [item for item in found if (item.ecu, item.source.key()) == route]
            specificity = max(item.applicability.specificity() for item in choices)
            best = [item for item in choices
                    if item.applicability.specificity() == specificity]
            if len(best) != 1:
                raise ConflictingDefinitionError(
                    f"Ambiguous implementations for {canonical_name} route {route}")
            selected.append(best[0])
        return tuple(selected)

    def decode_uds(self, ecu, did, payload, context, *, identity_verified=True):
        found = self.resolve_uds(ecu, did, context)
        if not found:
            raw_name = f"raw.unknown.{ecu}.{did:04x}"
            return {"did": f"{did:04X}", "signal": raw_name,
                    "canonical_name": raw_name, "unit": None,
                    "raw": payload.hex(), "value": None, "quality": "raw_only",
                    "validation_status": "CANDIDATE"}
        item = found[0]
        codec = dict(item.decoder) if item.decoder else None
        if codec is not None:
            verified_codec = (item.provenance.confidence == "confirmed" and
                              item.provenance.validated("raw_decode", context))
            codec["status"] = "confirmed" if verified_codec else "candidate"
        legacy = {"did": f"{did:04X}", "canonical_name": item.canonical_name,
                  "unit": item.unit, "validation_status": "CONFIRMED_LIVE"
                  if item.provenance.validated("raw_decode", context) else "CANDIDATE",
                  "raw": {"decoder": codec}}
        return decode_scalar(legacy, payload, identity_verified=identity_verified)


def _parse_source(data):
    kind = data.get("kind")
    did = data.get("did")
    if isinstance(did, str):
        did = int(did, 16)
    return SignalSource(kind=kind, did=did, database=data.get("database"),
                        message=data.get("message"), signal=data.get("signal"),
                        active_cost=data.get("active_cost"))


def _parse_validation_evidence(data):
    if not isinstance(data, Mapping):
        raise ValueError("Validation evidence must be an object")
    identity = data.get("identity")
    return ValidationEvidence(scope=data.get("scope", ""), method=data.get("method", ""),
        result=data.get("result", ""),
        identity=IdentityContext.from_mapping(identity) if identity is not None else None,
        date=data.get("date"), notes=data.get("notes", ""))


def definition_from_dict(data, *, default_provider="generic"):
    provenance = data.get("provenance", {})
    return SignalDefinition(
        id=data.get("id", ""), canonical_name=data.get("canonical_name", ""),
        ecu=data.get("ecu", ""), source=_parse_source(data.get("source", {})),
        provenance=Provenance(**provenance),
        provider_id=data.get("provider_id", default_provider),
        applicability=Applicability(data.get("applicability", {})),
        decoder=data.get("decoder"), metadata=data.get("metadata", {}),
        unit=data.get("unit"), ttl=data.get("ttl", 1.0),
        expected_period=data.get("expected_period"))


def load_registry(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or not isinstance(data.get("definitions"), list):
        raise ValueError("Unsupported signal registry schema")
    provider = data.get("provider_id", "generic")
    return SignalRegistry(definition_from_dict(item, default_provider=provider)
                          for item in data["definitions"])
