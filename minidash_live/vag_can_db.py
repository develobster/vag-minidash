"""Receive-only VAG candidate decoding from the generated firmware catalogue."""
import math
from .vag_can_data import DEFINITIONS, SIGNAL_NAMES


def unpack(data, start, bits, little=True, signed=False):
    if not 0 < len(data) <= 8 or not 1 <= bits <= 32 or not 0 <= start < len(data)*8:
        raise ValueError("Invalid CAN scalar")
    result, bit = 0, start
    for i in range(bits):
        if not 0 <= bit < len(data)*8:
            raise ValueError("Scalar outside payload")
        value = (data[bit//8] >> (bit % 8)) & 1
        result = result | (value << i) if little else (result << 1) | value
        bit = bit + 1 if little else bit + 15 if bit % 8 == 0 else bit - 1
    scalar = result - (1 << bits) if signed and result & (1 << (bits-1)) else result
    return result, scalar


class VagCanDatabase:
    def __init__(self, platform="MQB", layout="sourced"):
        platform = platform.upper()
        if layout == "experimental":
            layout = "requested_experimental"
        if platform not in ("MQB", "PQ35_46") or layout not in ("sourced", "requested_experimental"):
            raise ValueError("Unknown VAG platform/layout")
        self.platform, self.layout = platform, layout
        self.definitions = tuple(d for d in DEFINITIONS if (d["platform"], d["layout"]) == (platform, layout))
        self.samples, self.rejected = {}, 0

    def feed(self, can_id, data, stamp_ms, *, extended=False, remote=False, error=False):
        if type(can_id) is not int or not 0 <= can_id <= 0x7ff or extended or remote or error \
                or not 0 < len(data) <= 8 or type(stamp_ms) is not int or not 0 <= stamp_ms <= 0xffffffff:
            self.rejected += 1
            return 0
        count = 0
        for d in self.definitions:
            if d["can_id"] != can_id:
                continue
            if "mux_start" in d:
                _, mux = unpack(data, d["mux_start"], d["mux_bits"])
                if mux != d["mux_value"]:
                    continue
            valid = len(data) == d["dlc"]
            if valid:
                raw, value = unpack(data, d["start_bit"], d["bits"], d["little"], d["signed"])
                if "valid_bit" in d:
                    bit = d["valid_bit"]
                    valid = (data[bit//8] >> (bit % 8)) & 1 == d["valid_value"]
                valid = valid and raw != d.get("invalid_raw", -1)
                if "all_bits_mask" in d:
                    value = int(raw & d["all_bits_mask"] == d["all_bits_mask"])
                value = value*d["scale"] + d["offset"]
                if "sign_bit" in d and (data[d["sign_bit"]//8] >> (d["sign_bit"] % 8)) & 1:
                    value = -value
                if "term_start" in d:
                    _, term = unpack(data, d["term_start"], d["term_bits"])
                    value += term*d["term_scale"]
                valid = valid and math.isfinite(value) and d["minimum"] <= value <= d["maximum"]
            self.samples[d["signal"]] = dict(value=value if valid else None, stamp_ms=stamp_ms,
                ttl_ms=d["ttl_ms"], quality="candidate" if valid else "error", source=d["source"])
            if valid:
                count += 1
            else:
                self.rejected += 1
        return count

    def snapshot(self, now_ms):
        values = {}
        for signal in SIGNAL_NAMES:
            sample = self.samples.get(signal)
            if sample is None:
                values[signal] = dict(value=None, quality="unavailable")
                continue
            item = dict(sample)
            if item["quality"] != "error" and (now_ms-item["stamp_ms"]) & 0xffffffff >= item["ttl_ms"]:
                item.update(value=None, quality="stale")
            values[signal] = item
        return dict(platform=self.platform, layout=self.layout, telemetry=values,
                    vehicle_validation=False, rejected=self.rejected)
