"""Receive-only declarative CAN decoding. No supplied definitions describe VAG."""
from dataclasses import dataclass
import math
from .generated_can_signals import unpack_scalar


@dataclass(frozen=True)
class Field:
    ecu: str
    signal: str
    can_id: int
    extended: bool
    dlc: int
    offset: int
    length: int
    unit: str
    scale: float = 1
    bias: float = 0
    signed: bool = False
    byteorder: str = "big"
    invalid_raw: tuple = ()
    ttl: float = 1
    evidence: str = ""

    def __post_init__(self):
        if type(self.extended) is not bool or type(self.signed) is not bool:
            raise ValueError("Boolean flags required")
        if any(type(x) is not int for x in (self.can_id, self.dlc, self.offset, self.length)):
            raise ValueError("Integer frame layout required")
        if not 0 <= self.can_id <= (0x1fffffff if self.extended else 0x7ff):
            raise ValueError("CAN ID outside range")
        if not 1 <= self.dlc <= 8 or not 1 <= self.length <= 8 or not 0 <= self.offset <= self.dlc-self.length:
            raise ValueError("Field outside Classical CAN payload")
        if self.byteorder not in ("big", "little") or not self.evidence:
            raise ValueError("Byteorder and evidence required")
        if not all(math.isfinite(x) for x in (self.scale, self.bias, self.ttl)) or self.ttl <= 0:
            raise ValueError("Invalid scale/TTL")


class PassiveDecoder:
    def __init__(self, fields, store, *, profile_verified=False):
        self.fields, self.store, self.profile_verified = tuple(fields), store, profile_verified
        self.rejected = 0

    def feed(self, can_id, data, *, extended=False, remote=False, error=False):
        if remote or error:
            self.rejected += 1
            return 0
        count = 0
        for field in self.fields:
            if (field.can_id, field.extended) != (can_id, extended):
                continue
            quality, value = "identity_unverified", None
            if len(data) != field.dlc:
                quality = "invalid_length"
            elif self.profile_verified:
                raw = bytes(data[field.offset:field.offset+field.length])
                if raw.hex() in field.invalid_raw:
                    quality = "invalid"
                else:
                    # Same generated scalar contract as provisioned ESP32 bindings.
                    # Retain the legacy API's support for 64-bit byte fields.
                    if len(raw) <= 4:
                        value = unpack_scalar(raw, 0, len(raw)*8, field.byteorder == "little",
                                              field.signed, field.scale, field.bias)
                    else:
                        value = int.from_bytes(raw, field.byteorder, signed=field.signed)*field.scale+field.bias
                    quality = "valid"
            self.store.publish(field.ecu, field.signal, "can", value, field.ttl, quality, field.unit)
            count += 1
        return count

    def receive_once(self, bus, timeout=.05):
        """Use with an externally owned python-can Bus. Never sends any frame.

        Hardware listen-only configuration remains the operator's responsibility.
        This must not compete with another bus.recv consumer: use a notifier or
        a separate receive socket when ISO-TP shares the physical interface.
        """
        msg = bus.recv(timeout)
        if msg is None:
            return 0
        return self.feed(msg.arbitration_id, msg.data, extended=msg.is_extended_id,
                         remote=msg.is_remote_frame, error=msg.is_error_frame)
