"""Profile catalog, evidence validation and deliberately conservative decoding."""
from collections.abc import Mapping
import json
import math
from pathlib import Path
import struct

VALID_STATUSES = {"CONFIRMED_LIVE", "CONFIRMED_ODIS", "CANDIDATE", "UNSUPPORTED"}
LEGACY_STATUSES = {"confirmed", "candidate", "unsupported"}
FINGERPRINT_FIELDS = ("sw_part", "sw_version", "hw_part", "hw_version", "asam_id", "asam_version")


def _normal(value):
    return "" if value is None else str(value).strip().upper()


def validate_codec(codec):
    """Validate the shared scalar codec used by profiles and the signal registry."""
    required = {"length", "byteorder", "signed", "scale", "offset", "source"}
    if not isinstance(codec, Mapping) or not required <= codec.keys():
        raise ValueError("Incomplete scalar decoder")
    length = codec["length"]
    if type(length) is not int or not 1 <= length <= 8:
        raise ValueError("Invalid decoder length")
    if codec["byteorder"] not in ("big", "little") or type(codec["signed"]) is not bool:
        raise ValueError("Invalid decoder representation")
    if not codec["source"] or not all(isinstance(codec[x], (int, float)) and
            not isinstance(codec[x], bool) and math.isfinite(codec[x]) for x in ("scale", "offset")):
        raise ValueError("Invalid decoder scaling/evidence")
    representation = codec.get("representation", "integer")
    if representation == "integer":
        return
    if representation == "float32":
        if length != 4 or codec["signed"] or codec.get("choices"):
            raise ValueError("Invalid float32 decoder")
        return
    if representation != "texttable" or codec["scale"] != 1 or codec["offset"] != 0:
        raise ValueError("Invalid decoder representation")
    choices = codec.get("choices")
    if not isinstance(choices, (list, tuple)) or not choices:
        raise ValueError("Invalid TEXTTABLE choices")
    bits = length * 8
    minimum = -(1 << (bits - 1)) if codec["signed"] else 0
    maximum = (1 << (bits - (1 if codec["signed"] else 0))) - 1
    previous_upper = minimum - 1
    for choice in choices:
        if not isinstance(choice, Mapping):
            raise ValueError("Invalid TEXTTABLE choice")
        lower, upper, label = (choice.get("lower"), choice.get("upper"),
                               choice.get("label"))
        if type(lower) is not int or type(upper) is not int or not isinstance(label, str) \
                or not label.strip() or not minimum <= lower <= upper <= maximum \
                or lower <= previous_upper:
            raise ValueError("Invalid TEXTTABLE choice")
        previous_upper = upper


def signals(profile):
    """Return v2 signals or the retained v1 DID list."""
    return profile.get("signals", profile.get("dids", []))


def load_profile(path):
    profile = json.loads(Path(path).read_text(encoding="utf-8"))
    schema = profile.get("schema_version")
    if schema not in (1, 2):
        raise ValueError("Unsupported profile schema")
    if not profile.get("id"):
        raise ValueError("Profile id required")
    seen = set()
    for item in signals(profile):
        key = item.get("did")
        if not isinstance(key, str) or len(key) != 4 or key in seen:
            raise ValueError("Invalid/duplicate DID")
        try:
            int(key, 16)
        except ValueError as exc:
            raise ValueError("Invalid/duplicate DID") from exc
        seen.add(key)
        status = item.get("validation_status", item.get("status"))
        if status not in VALID_STATUSES | LEGACY_STATUSES:
            raise ValueError("Invalid evidence status")
        if schema == 2:
            for field in ("name", "canonical_name", "source"):
                if not item.get(field):
                    raise ValueError(f"Signal {key} lacks {field}")
            raw = item.get("raw")
            if not isinstance(raw, dict) or "decoder" not in raw:
                raise ValueError(f"Signal {key} lacks raw decoder evidence")
        codec = item.get("decoder") or item.get("raw", {}).get("decoder")
        if codec:
            # Legacy profile decoders omitted signed and defaulted to unsigned.
            validate_codec(dict(codec, signed=codec.get("signed", False)))
    return profile


def resolve_signal(profile, did):
    key = f"{int(did, 16) if isinstance(did, str) else did:04X}"
    for item in signals(profile):
        if item["did"].upper() == key:
            return item
    return {"did": key, "name": "Unknown RDID", "canonical_name": f"unknown.did.{key}",
            "signal": f"unknown.did.{key}", "unit": None, "validation_status": "CANDIDATE",
            "status": "candidate", "source": "runtime raw probe", "raw": {"decoder": None},
            "notes": "Unknown DID: saved as raw evidence only."}


def decode(item, payload, *, identity_verified=True):
    status = item.get("validation_status", item.get("status"))
    name = item.get("canonical_name", item.get("signal"))
    codec = item.get("decoder") or item.get("raw", {}).get("decoder")
    result = {"did": item["did"], "signal": name, "canonical_name": name,
              "unit": item.get("unit"), "raw": payload.hex(), "value": None,
              "quality": "raw_only", "validation_status": status}
    if status in ("UNSUPPORTED", "unsupported"):
        result["quality"] = "unsupported"
        return result
    if not codec:
        return result
    if not identity_verified:
        result["quality"] = "identity_unverified"
        return result
    if len(payload) != codec["length"]:
        result["quality"] = "invalid_length"
        return result
    if payload.hex() in codec.get("invalid_raw", []):
        result["quality"] = "invalid"
        return result
    representation = codec.get("representation", "integer")
    if representation == "float32":
        value = struct.unpack(">f" if codec["byteorder"] == "big" else "<f", payload)[0]
    else:
        value = int.from_bytes(payload, codec["byteorder"], signed=codec.get("signed", False))
    if representation == "texttable":
        result["coded_value"] = value
        match = next((choice for choice in codec["choices"]
                      if choice["lower"] <= value <= choice["upper"]), None)
        if match is None:
            result["quality"] = "invalid"
            return result
        result.update(value=match["label"],
                      quality="valid" if codec.get("status") == "confirmed" else "candidate")
        return result
    value = value * codec["scale"] + codec["offset"]
    limits = codec.get("range")
    if not math.isfinite(value) or (limits and not limits[0] <= value <= limits[1]):
        result["quality"] = "invalid"
        return result
    result.update(value=value, quality="valid" if codec.get("status") == "confirmed" else "candidate")
    return result


def match_fingerprint(profile, observed):
    """Exact match only. Missing observed values never count as a match."""
    expected = profile.get("fingerprint", {})
    required = [field for field in FINGERPRINT_FIELDS if expected.get(field)]
    return bool(required) and all(_normal(observed.get(field)) == _normal(expected[field]) for field in required)


def match_catalog(profile_paths, observed):
    matches = []
    for path in profile_paths:
        profile = load_profile(path)
        if match_fingerprint(profile, observed):
            matches.append(profile)
    return matches
