"""Single-flight, read-only UDS 0x22 over complete diagnostic PDUs."""
from dataclasses import dataclass
import math
import threading
import time
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
        with self.lock:
            if self.tainted:
                raise ProtocolError("Session desynchronized; close/reopen transport before further reads")
            time.sleep(max(0, self.next_request - time.monotonic()))
            request = b"\x22" + did.to_bytes(2, "big")
            deadline = time.monotonic() + self.total
            pending = 0
            try:
                self.trace({"kind": "pdu", "direction": "tx", "pdu": request.hex()})
                self.transport.send(request, min(self.p2, self.total))
                wait = self.p2
                while True:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("UDS total deadline exceeded")
                    response = self.transport.recv(min(wait, remaining))
                    if response is None:
                        raise TimeoutError("UDS response timed out")
                    self.trace({"kind": "pdu", "direction": "rx", "pdu": response.hex()})
                    if response[:1] == b"\x7f":
                        if len(response) != 3 or response[1] != 0x22:
                            raise ProtocolError("Malformed/unrelated negative response")
                        if response[2] == 0x78:
                            pending += 1
                            wait = self.p2_star
                            continue
                        raise NegativeResponse(response[2])
                    if len(response) < 4 or response[:3] != b"\x62" + request[1:]:
                        raise ProtocolError("Wrong SID/DID echo or empty DID payload")
                    return UdsExchange(0x22, did, request, response, response[3:], pending)
            except NegativeResponse:
                raise
            except Exception:
                self.tainted = True
                raise
            finally:
                self.next_request = time.monotonic() + self.interval
