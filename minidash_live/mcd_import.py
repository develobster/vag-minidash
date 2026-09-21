"""Conservative importer for the simple subset of mcd-diag-rs MWB JSON.

Complex structures are reported, never approximated.  The input is a local
artifact produced by the operator; proprietary ODIS/MCD databases are not
distributed by MiniDash.
"""
from dataclasses import dataclass
import json
from pathlib import Path
import re

from .registry import Provenance, SignalDefinition, SignalSource


@dataclass(frozen=True)
class ImportReport:
    definitions: tuple
    unsupported: tuple


_LINEAR = re.compile(
    r"^\s*\(\s*([-+0-9.eE]+)\s*\+\s*\(\s*x\s*\*\s*([-+0-9.eE]+)\s*\)\s*\)"
    r"\s*/\s*([-+0-9.eE]+)\s*$")


def _slug(value):
    slug = re.sub(r"[^a-z0-9]+", ".", str(value).lower()).strip(".")
    return slug or "value"


def _linear(dop):
    category = dop.get("compu_category", "IDENTICAL").upper()
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


def _invalid_raw(dop, length, byteorder):
    result = []
    constraint = dop.get("internal_constraint") or {}
    for item in constraint.get("scale_constraints") or []:
        if item.get("validity") not in ("NOT-AVAILABLE", "INVALID"):
            continue
        lower, upper = item.get("lower_limit") or {}, item.get("upper_limit") or {}
        if lower.get("type") == upper.get("type") == "CLOSED" and lower.get("value") == upper.get("value"):
            value = lower["value"]
            if isinstance(value, int) and 0 <= value < 1 << (length * 8):
                result.append(value.to_bytes(length, byteorder).hex())
    return result


def _convert(record, *, ecu, applicability, provenance):
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
    if base not in ("A_UINT32", "A_INT32", "A_BITFIELD"):
        raise ValueError(f"unsupported coded type {base}")
    byteorder = "little" if str(dop.get("endianness", "big")).lower() in ("little", "lsb") else "big"
    scale, offset = _linear(dop)
    units = dop.get("units") or {}
    unit = units.get("display_name") or units.get("long_name")
    name = (param.get("translated_name") or param.get("long_name") or
            record.get("translated_name") or record.get("long_name") or f"did_{did:04x}")
    codec = {"length": length, "byteorder": byteorder,
             "signed": base == "A_INT32" or dop.get("encoding") == "2C",
             "scale": scale, "offset": offset,
             "invalid_raw": _invalid_raw(dop, length, byteorder),
             "source": provenance.source_url}
    return SignalDefinition(
        id=f"mcd.{ecu}.{did:04x}.{_slug(name)}", canonical_name=f"diagnostic.{ecu}.{did:04x}.{_slug(name)}",
        ecu=ecu, source=SignalSource("uds", did=did), provenance=provenance,
        applicability=dict(applicability), decoder=codec, unit=unit, ttl=1.0)


def import_mcd_mwb(path, *, ecu, applicability, source_url=None):
    """Import simple MWB entries from mcd-diag-rs JSON; report every rejection."""
    path = Path(path)
    records = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(records, list):
        raise ValueError("MWB JSON must be a top-level array")
    provenance = Provenance(origin="odis_mcd", confidence="documented",
        source_url=source_url or path.name, vehicle_tested=False,
        verification_scope="database_definition",
        notes="Locally extracted MCD definition; live ECU/raw validation is still required.")
    definitions, unsupported = [], []
    for index, record in enumerate(records):
        try:
            definitions.append(_convert(record, ecu=ecu, applicability=applicability,
                                        provenance=provenance))
        except (KeyError, TypeError, ValueError, OverflowError) as exc:
            unsupported.append({"index": index, "did": record.get("did")
                                if isinstance(record, dict) else None, "reason": str(exc)})
    return ImportReport(tuple(definitions), tuple(unsupported))
