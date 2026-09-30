"""Conservative importer for single-value mcd-diag-rs MWB JSON.

Complex structures are reported, never approximated.  The input is a local
artifact produced by the operator; proprietary ODIS/MCD databases are not
distributed by MiniDash.
"""
from dataclasses import dataclass
import json
from pathlib import Path
import re

from .identity import Applicability
from .registry import Provenance, SignalDefinition, SignalSource


@dataclass(frozen=True)
class ImportReport:
    definitions: tuple
    unsupported: tuple


@dataclass(frozen=True)
class McdVariantMetadata:
    """Relationship carried by an extracted MCD base/ECU-variant artifact."""
    provider_id: str = "vag"
    manufacturer: str | None = None
    platform: str | None = None
    base_variant: str | None = None
    ecu_variant: str | None = None
    ecu_family: str | None = None
    asam_id: str | None = None
    asam_version: str | None = None
    part_number: str | None = None
    software_version: str | None = None

    @classmethod
    def from_mapping(cls, data):
        if isinstance(data, cls):
            return data
        if not isinstance(data, dict):
            raise TypeError("MCD variant metadata must be an object")
        aliases = {"sw_part": "part_number", "sw_version": "software_version"}
        allowed = set(cls.__dataclass_fields__)
        normalized = {aliases.get(key, key): value for key, value in data.items()}
        unknown = set(normalized) - allowed
        if unknown:
            raise ValueError(f"Unknown MCD variant metadata: {sorted(unknown)}")
        return cls(**normalized)

    def applicability(self):
        values = {key: getattr(self, key) for key in (
            "manufacturer", "platform", "ecu_family", "asam_id", "asam_version",
            "part_number", "software_version") if getattr(self, key) is not None}
        return Applicability(values)

    def relationship(self):
        return {key: value for key, value in (
            ("mcd_base_variant", self.base_variant),
            ("mcd_ecu_variant", self.ecu_variant),
        ) if value is not None}


_LINEAR = re.compile(
    r"^\s*\(\s*([-+0-9.eE]+)\s*\+\s*\(\s*x\s*\*\s*([-+0-9.eE]+)\s*\)\s*\)"
    r"\s*/\s*([-+0-9.eE]+)\s*$")


def _slug(value):
    slug = re.sub(r"[^a-z0-9]+", ".", str(value).lower()).strip(".")
    return slug or "value"


def _linear(dop):
    category = str(dop.get("compu_category") or "IDENTICAL").upper()
    if category == "IDENTICAL":
        return 1.0, 0.0
    if category not in ("LINEAR", "SCALE-LINEAR") or not dop.get("compu_scale"):
        raise ValueError(f"unsupported compu category {category}")
    formula = dop["compu_scale"].get("formula", "")
    match = _LINEAR.fullmatch(formula)
    if not match:
        raise ValueError("unsupported linear formula")
    bias, factor, divisor = (float(value) for value in match.groups())
    if divisor == 0:
        raise ValueError("zero formula divisor")
    return factor / divisor, bias / divisor


def _invalid_raw(dop, length, byteorder, signed=False):
    result = []
    constraint = dop.get("internal_constraint") or {}
    for item in constraint.get("scale_constraints") or []:
        if item.get("validity") not in ("NOT-AVAILABLE", "INVALID"):
            continue
        lower, upper = item.get("lower_limit") or {}, item.get("upper_limit") or {}
        if lower.get("type") == upper.get("type") == "CLOSED" and lower.get("value") == upper.get("value"):
            value = lower["value"]
            lower_bound = -(1 << (length * 8 - 1)) if signed else 0
            upper_bound = (1 << (length * 8 - (1 if signed else 0))) - 1
            if type(value) is int and lower_bound <= value <= upper_bound:
                result.append(value.to_bytes(length, byteorder, signed=signed).hex())
    return result


def _texttable(dop, bits, signed):
    if dop.get("compu_default_value") is not None:
        raise ValueError("TEXTTABLE default value is unsupported")
    scales = dop.get("compu_scales") or []
    if not isinstance(scales, (list, tuple)) or not scales:
        raise ValueError("TEXTTABLE has no choices")
    minimum = -(1 << (bits - 1)) if signed else 0
    maximum = (1 << (bits - (1 if signed else 0))) - 1
    choices = []
    for scale in scales:
        if not isinstance(scale, dict):
            raise ValueError("invalid TEXTTABLE choice")
        lower, upper, label = (scale.get("lower_limit"), scale.get("upper_limit"),
                               scale.get("long_name"))
        if type(lower) is not int or type(upper) is not int or not isinstance(label, str) \
                or not label.strip() or not minimum <= lower <= upper <= maximum:
            raise ValueError("invalid TEXTTABLE choice")
        choices.append({"lower": lower, "upper": upper, "label": label})
    choices.sort(key=lambda item: item["lower"])
    if any(left["upper"] >= right["lower"] for left, right in zip(choices, choices[1:])):
        raise ValueError("overlapping TEXTTABLE choices")
    return choices


def _convert(record, *, ecu, applicability, provenance, provider_id,
             relationship, canonical_map):
    did = record.get("did")
    if type(did) is not int or not 0 <= did <= 0xffff:
        raise ValueError("invalid DID")
    structure = record.get("structure") or {}
    params = [item for item in structure.get("parameters", [])
              if item.get("parameter_type") == "VALUE"]
    if len(params) != 1:
        raise ValueError("only one VALUE parameter is supported")
    param = params[0]
    dop = param.get("dop") or {}
    if dop.get("type") != "DOP" or dop.get("diag_coded_type") != "STANDARD-LENGTH-TYPE":
        raise ValueError("only fixed leaf DOP is supported")
    bits = dop.get("bit_length")
    if type(bits) is not int or bits % 8 or not 8 <= bits <= 64 or param.get("bit_position", 0) != 0:
        raise ValueError("only byte-aligned 1..8 byte values are supported")
    if param.get("byte_position") not in (0, None):
        raise ValueError("nonzero byte position requires a structured decoder")
    length = bits // 8
    if structure.get("byte_size") not in (None, length):
        raise ValueError("structure size does not match the scalar")
    base = dop.get("coded_base_data_type")
    if base not in ("A_UINT32", "A_INT32", "A_BITFIELD", "A_FLOAT32"):
        raise ValueError(f"unsupported coded type {base}")
    order = str(dop.get("endianness") or "big").lower()
    if order not in ("big", "msb", "little", "lsb"):
        raise ValueError("unsupported scalar endianness")
    byteorder = "little" if order in ("little", "lsb") else "big"
    signed = base == "A_INT32" or dop.get("encoding") == "2C"
    representation = "integer"
    if base == "A_FLOAT32":
        if bits != 32 or dop.get("encoding") != "IEEE754":
            raise ValueError("unsupported A_FLOAT32 encoding")
        representation = "float32"
        signed = False
    if str(dop.get("compu_category", "IDENTICAL")).upper() == "TEXTTABLE":
        if representation != "integer":
            raise ValueError("TEXTTABLE requires an integer coded type")
        representation = "texttable"
        choices = _texttable(dop, bits, signed)
        scale, offset = 1.0, 0.0
    else:
        scale, offset = _linear(dop)
    units = dop.get("units") or {}
    unit = units.get("display_name") or units.get("long_name")
    name = (param.get("translated_name") or param.get("long_name") or
            record.get("translated_name") or record.get("long_name") or f"did_{did:04x}")
    mapped = (canonical_map.get(did) or canonical_map.get(f"{did:04X}") or
              canonical_map.get(name))
    canonical_name = mapped or f"raw.{_slug(provider_id)}.{_slug(ecu)}.{did:04x}.{_slug(name)}"
    codec = {"length": length, "byteorder": byteorder,
             "signed": signed,
             "scale": scale, "offset": offset,
             "invalid_raw": _invalid_raw(dop, length, byteorder, signed),
             "source": provenance.source_url}
    if representation != "integer":
        codec["representation"] = representation
    if representation == "texttable":
        codec["choices"] = choices
    return SignalDefinition(
        id=f"mcd.{_slug(provider_id)}.{_slug(ecu)}.{did:04x}.{_slug(name)}",
        canonical_name=canonical_name,
        ecu=ecu, source=SignalSource("uds", did=did), provenance=provenance,
        provider_id=provider_id, applicability=applicability, decoder=codec,
        metadata=relationship, unit=unit, ttl=1.0)


def import_mcd_mwb(path, *, ecu, variant=None, applicability=None, source_url=None,
                   provider_id=None, canonical_map=None):
    """Import simple MWB entries and preserve available variant relationships.

    A top-level list remains supported.  A new envelope may contain
    ``{"metadata": {...}, "records": [...]}``, allowing the extractor to carry
    base variant, ECU variant and identity selectors without caller duplication.
    """
    path = Path(path)
    document = json.loads(path.read_text(encoding="utf-8"))
    embedded = None
    if isinstance(document, dict):
        records = document.get("records")
        embedded = document.get("metadata")
    else:
        records = document
    if not isinstance(records, list):
        raise ValueError("MWB JSON must be an array or metadata/records envelope")
    if variant is not None and embedded is not None:
        raise ValueError("Variant metadata is present in both artifact and call")
    variant = McdVariantMetadata.from_mapping(embedded or variant or {})
    if provider_id is not None and variant.provider_id != provider_id:
        if embedded or variant != McdVariantMetadata():
            raise ValueError("Conflicting MCD provider ids")
        variant = McdVariantMetadata(provider_id=provider_id)
    provider_id = provider_id or variant.provider_id
    selectors = variant.applicability().as_dict()
    if applicability is not None:
        for key, value in Applicability(applicability).items():
            if key in selectors and selectors[key] != value:
                raise ValueError(f"Conflicting MCD applicability selector: {key}")
            selectors[key] = value
    applicability = Applicability(selectors)
    canonical_map = dict(canonical_map or {})
    provenance = Provenance(origin="odis_mcd", confidence="documented",
        source_url=source_url or path.name, vehicle_tested=False,
        verification_scope="database_definition",
        notes="Locally extracted MCD definition; live ECU/raw validation is still required.")
    definitions, unsupported = [], []
    for index, record in enumerate(records):
        try:
            definitions.append(_convert(record, ecu=ecu, applicability=applicability,
                                        provenance=provenance, provider_id=provider_id,
                                        relationship=variant.relationship(),
                                        canonical_map=canonical_map))
        except (KeyError, TypeError, ValueError, OverflowError) as exc:
            unsupported.append({"index": index, "did": record.get("did")
                                if isinstance(record, dict) else None, "reason": str(exc)})
    return ImportReport(tuple(definitions), tuple(unsupported))
