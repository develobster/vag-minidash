"""Vehicle/ECU identity and semantic applicability matching.

Identity is deliberately independent of VAG concepts.  Legacy profile field
names (``sw_part`` etc.) are accepted at the JSON boundary, not propagated
through the core API.
"""
from collections.abc import Mapping
from dataclasses import dataclass, fields


def _normal(value):
    if value is None:
        return None
    if isinstance(value, bool):
        raise ValueError("Boolean is not a valid identity value")
    value = str(value).strip().upper()
    return value or None


@dataclass(frozen=True)
class VehicleIdentity:
    manufacturer: str | None = None
    platform: str | None = None
    model: str | None = None
    model_year: str | int | None = None
    vin: str | None = None

    def __post_init__(self):
        for item in fields(self):
            object.__setattr__(self, item.name, _normal(getattr(self, item.name)))


@dataclass(frozen=True)
class EcuIdentity:
    logical_role: str | None = None
    diagnostic_address: str | int | None = None
    request_id: str | int | None = None
    response_id: str | int | None = None
    part_number: str | None = None
    software_version: str | None = None
    hardware_part_number: str | None = None
    hardware_version: str | None = None
    asam_id: str | None = None
    asam_version: str | None = None
    ecu_family: str | None = None
    supplier: str | None = None

    def __post_init__(self):
        for item in fields(self):
            object.__setattr__(self, item.name, _normal(getattr(self, item.name)))


@dataclass(frozen=True)
class IdentityContext:
    vehicle: VehicleIdentity = VehicleIdentity()
    ecu: EcuIdentity = EcuIdentity()

    @classmethod
    def from_mapping(cls, values):
        """Compatibility adapter for schema-v1 flat selector dictionaries."""
        if isinstance(values, cls):
            return values
        if not isinstance(values, Mapping):
            raise TypeError("Identity must be IdentityContext or a mapping")
        if "vehicle" in values or "ecu" in values:
            unknown = set(values) - {"vehicle", "ecu"}
            if unknown:
                raise ValueError("Nested identity cannot contain flat selectors")
            vehicle_values = values.get("vehicle", {})
            ecu_values = values.get("ecu", {})
            if not isinstance(vehicle_values, Mapping) or not isinstance(ecu_values, Mapping):
                raise TypeError("Nested vehicle and ECU identities must be objects")
            flattened = dict(vehicle_values)
            for name, value in ecu_values.items():
                if name in flattened:
                    raise ValueError(f"Duplicate nested identity selector: {name}")
                flattened[name] = value
            return cls.from_mapping(flattened)
        aliases = {
            "sw_part": "part_number", "sw_version": "software_version",
            "hw_part": "hardware_part_number", "hw_version": "hardware_version",
            "year": "model_year", "ecu": "logical_role",
        }
        vehicle_names = {item.name for item in fields(VehicleIdentity)}
        ecu_names = {item.name for item in fields(EcuIdentity)}
        vehicle, ecu = {}, {}
        for raw_name, value in values.items():
            name = aliases.get(raw_name, raw_name)
            if name in vehicle_names:
                vehicle[name] = value
            elif name in ecu_names:
                ecu[name] = value
            else:
                raise ValueError(f"Unknown identity selector: {raw_name}")
        return cls(VehicleIdentity(**vehicle), EcuIdentity(**ecu))

    def value(self, name):
        if hasattr(self.vehicle, name):
            return getattr(self.vehicle, name)
        if hasattr(self.ecu, name):
            return getattr(self.ecu, name)
        raise KeyError(name)


SELECTOR_ALIASES = {
    "sw_part": "part_number", "sw_version": "software_version",
    "hw_part": "hardware_part_number", "hw_version": "hardware_version",
    "year": "model_year", "ecu": "logical_role",
}
VEHICLE_SELECTORS = {item.name for item in fields(VehicleIdentity)}
ECU_SELECTORS = {item.name for item in fields(EcuIdentity)}
ALL_SELECTORS = VEHICLE_SELECTORS | ECU_SELECTORS


@dataclass(frozen=True, order=True)
class Specificity:
    """Semantic order; field count is only the final within-scope discriminator."""
    scope: int
    exact_fingerprint: int
    software_identity: int
    diagnostic_variant: int
    family_identity: int
    vehicle_identity: int
    platform_identity: int
    manufacturer_identity: int
    selector_count: int


class Applicability:
    """Immutable selectors with safe matching and semantic specificity."""
    __slots__ = ("_selectors",)

    def __init__(self, selectors=None):
        selectors = selectors or {}
        if isinstance(selectors, Applicability):
            self._selectors = selectors._selectors
            return
        if not isinstance(selectors, Mapping):
            raise TypeError("Applicability selectors must be a mapping")
        normalized = {}
        for raw_name, value in selectors.items():
            name = SELECTOR_ALIASES.get(raw_name, raw_name)
            if name not in ALL_SELECTORS:
                raise ValueError(f"Unknown applicability selector: {raw_name}")
            value = _normal(value)
            if value is None:
                raise ValueError(f"Applicability selector {raw_name} cannot be empty")
            if name in normalized and normalized[name] != value:
                raise ValueError(f"Conflicting alias for selector {name}")
            normalized[name] = value
        self._selectors = tuple(sorted(normalized.items()))

    def __len__(self):
        return len(self._selectors)

    def __iter__(self):
        return (name for name, _ in self._selectors)

    def items(self):
        return self._selectors

    def as_dict(self):
        return dict(self._selectors)

    def matches(self, identity):
        identity = IdentityContext.from_mapping(identity)
        # Missing observed identity is a mismatch, never a wildcard.
        return all(identity.value(name) is not None and identity.value(name) == value
                   for name, value in self._selectors)

    def specificity(self):
        names = {name for name, _ in self._selectors}
        exact = int({"part_number", "software_version"} <= names and bool(
            names & {"hardware_part_number", "hardware_version", "asam_id"}))
        software = sum(name in names for name in (
            "part_number", "software_version", "hardware_part_number", "hardware_version"))
        diagnostic = sum(name in names for name in ("asam_id", "asam_version"))
        family = sum(name in names for name in ("ecu_family", "supplier"))
        vehicle = sum(name in names for name in ("vin", "model", "model_year"))
        platform = int("platform" in names)
        manufacturer = int("manufacturer" in names)

        if exact:
            scope = 7
        elif ({"part_number", "software_version"} <= names or
              {"asam_id", "software_version"} <= names or
              {"asam_id", "asam_version"} <= names):
            scope = 6
        elif diagnostic:
            scope = 5
        elif vehicle:
            scope = 4
        elif family:
            scope = 3
        elif platform:
            scope = 2
        elif manufacturer:
            scope = 1
        else:
            scope = 0
        return Specificity(scope, exact, software, diagnostic, family, vehicle,
                           platform, manufacturer, len(names))

    def __eq__(self, other):
        return isinstance(other, Applicability) and self._selectors == other._selectors

    def __hash__(self):
        return hash(self._selectors)

    def __repr__(self):
        return f"Applicability({dict(self._selectors)!r})"
