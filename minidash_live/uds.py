"""Single-flight, read-only UDS 0x22 over complete diagnostic PDUs."""
import math
import threading
import time
from dataclasses import dataclass
from typing import Protocol


class Transport(Protocol):
    def send(self, pdu: bytes, timeout: float): ...
    def recv(self, timeout: float) -> bytes | None: ...
    def close(self): ...


class ProtocolError(Exception):
    pass


class NegativeResponse(ProtocolError):
    def __init__(self, code):
        self.code = code
        super().__init__(f"UDS NRC 0x{code:02X}")


@dataclass(frozen=True)
class UdsExchange:
    service: int
    did: int
    request: bytes
    response: bytes
    payload: bytes
    pending_count: int


def pdu_log_fields(pdu, *, request=None):
    """Return trace-safe PDU fields; F190/VIN payloads are never persisted."""
    pdu = bytes(pdu)
    if request == b"\x22\xf1\x90" and pdu[:1] != b"\x7f":
        return {
            "pdu": pdu[:3].hex(),
            "payload_redacted": "vehicle.vin",
            "payload_length": max(0, len(pdu) - 3),
        }
    return {"pdu": pdu.hex()}


class ReadClient:
    def __init__(self, transport, trace=lambda event: None, p2=1.0, p2_star=5.0, total=10.0, interval=.2):
        for value in (p2, p2_star, total, interval):
            if type(value) is bool or not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
                raise ValueError("Timing must be positive and finite")
        self.transport, self.trace = transport, trace
        self.p2, self.p2_star, self.total, self.interval = p2, p2_star, total, interval
        self.lock = threading.Lock()
        self.next_request = 0.0
        self.tainted = False

    def read_did(self, did):
        return self.read_did_exchange(did).payload

    def read_did_exchange(self, did):
        if type(did) is not int or not 0 <= did <= 65535:
            raise ValueError("DID must be a 16-bit integer")
        request = b"\x22" + did.to_bytes(2, "big")
        response, pending = self._exchange(request)
        return UdsExchange(0x22, did, request, response, response[3:], pending)

    def session_control(self, session=1, *, allow_extended=False):
        if type(session) is not int or session not in (1, 3) or (session == 3 and not allow_extended):
            raise ValueError("Only default or explicitly enabled extended session allowed")
        response, _ = self._exchange(bytes([0x10, session]))
        return response

    def tester_present(self):
        return self._exchange(b"\x3e\x00")[0]

    def _exchange(self, request):
        if not ((len(request) == 3 and request[0] == 0x22) or
                request in (b"\x10\x01", b"\x10\x03", b"\x3e\x00")):
            raise ValueError("Service outside read/session allowlist")
        with self.lock:
            if self.tainted:
                raise ProtocolError("Session desynchronized; close/reopen transport before further reads")
            time.sleep(max(0, self.next_request - time.monotonic()))
            deadline = time.monotonic() + self.total
            pending = 0
            try:
                self.trace({"kind": "pdu", "direction": "tx",
                            **pdu_log_fields(request)})
                self.transport.send(request, min(self.p2, self.total))
                wait = self.p2
                while True:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("UDS total deadline exceeded")
                    response = self.transport.recv(min(wait, remaining))
                    if response is None:
                        raise TimeoutError("UDS response timed out")
                    self.trace({"kind": "pdu", "direction": "rx",
                                **pdu_log_fields(response, request=request)})
                    if response[:1] == b"\x7f":
                        if len(response) != 3 or response[1] != request[0]:
                            raise ProtocolError("Malformed/unrelated negative response")
                        if response[2] == 0x78:
                            pending += 1
                            wait = self.p2_star
                            continue
                        raise NegativeResponse(response[2])
                    if response[:len(request)] != bytes([request[0] + 0x40]) + request[1:]:
                        raise ProtocolError("Wrong response SID/echo")
                    if request[0] == 0x22 and len(response) < 4:
                        raise ProtocolError("Empty DID payload")
                    if request[0] == 0x3e and len(response) != 2:
                        raise ProtocolError("Malformed TesterPresent")
                    if request[0] == 0x10:
                        if len(response) != 6:
                            raise ProtocolError("Expected modern UDS session timing record")
                        p2 = int.from_bytes(response[2:4], "big") / 1000
                        p2_star = int.from_bytes(response[4:6], "big") / 100
                        if not 0 < p2 <= self.total or not 0 < p2_star <= self.total:
                            raise ProtocolError("Session timings exceed local deadline or are zero")
                        self.p2, self.p2_star = p2, p2_star
                    return response, pending
            except NegativeResponse:
                raise
            except Exception:
                self.tainted = True
                raise
            finally:
                self.next_request = time.monotonic() + self.interval
