"""Bounded read-only diagnostic lab; complete PDUs, no CAN/ISO-TP transport.

The client is intentionally restricted to FakeECU. This is a test harness, not
a complete ISO 14229 stack or a replacement for udsoncan in a live backend.
"""
from collections import deque
from dataclasses import dataclass
import json
import math


class DiagnosticError(Exception):
    pass


class ResponseError(DiagnosticError):
    pass


class DiagnosticTimeout(DiagnosticError):
    pass


class NegativeResponse(DiagnosticError):
    def __init__(self, sid, code):
        self.sid, self.code = sid, code
        super().__init__(f'Negative response to 0x{sid:02X}: NRC 0x{code:02X}')


@dataclass(frozen=True)
class Reading:
    pid: int
    name: str
    value: float
    unit: str


# Standard Mode 01 payload codecs. Exactly one PID per PDU; no CAN padding here.
PID_FORMATS = {
    0x04: ('engine_load', 1, 100 / 255, 0, '%'),
    0x05: ('coolant_temperature', 1, 1, -40, '°C'),
    0x0C: ('engine_rpm', 2, .25, 0, 'rpm'),
    0x0D: ('vehicle_speed', 1, 1, 0, 'km/h'),
    0x0F: ('intake_temperature', 1, 1, -40, '°C'),
    0x11: ('throttle_position', 1, 100 / 255, 0, '%'),
    0x42: ('control_module_voltage', 2, .001, 0, 'V'),
    0x5C: ('engine_oil_temperature', 1, 1, -40, '°C'),
}


def decode_pid(pid, pdu):
    if pid not in PID_FORMATS:
        raise ValueError('PID decoder not implemented')
    name, length, factor, offset, unit = PID_FORMATS[pid]
    if not isinstance(pdu, bytes) or len(pdu) != 2 + length or pdu[:2] != bytes([0x41, pid]):
        raise ResponseError('PID response service, echo or length mismatch')
    value = int.from_bytes(pdu[2:], 'big') * factor + offset
    return Reading(pid, name, value, unit)


def decode_support(base, pdu):
    if type(base) is not int or base not in range(0, 0xE1, 0x20):
        raise ValueError('Invalid support-page base')
    if not isinstance(pdu, bytes) or len(pdu) != 6 or pdu[:2] != bytes([0x41, base]):
        raise ResponseError('Invalid support bitmap')
    bits = int.from_bytes(pdu[2:], 'big')
    return {base + n for n in range(1, 33) if bits & (1 << (32 - n))}


class FakeECU:
    """Deterministic virtual-time ECU. send/recv only manipulate memory.

    faults: normal, pending, endless_pending, silent, negative, wrong_echo,
    truncated. Addresses and values are synthetic, not a real vehicle profile.
    """
    MODES = ('normal', 'pending', 'endless_pending', 'silent', 'negative', 'wrong_echo', 'truncated')

    def __init__(self, mode='normal'):
        if mode not in self.MODES:
            raise ValueError('Unknown fake ECU mode')
        self.mode = mode
        self.now = 0.0
        self.queue = deque()
        self.requests = []
        self.data = {0xF190: b'WVWZZZ00000000000'}  # fictional, 17 bytes
        self.pids = {0x04: b'\x80', 0x05: b'\x82', 0x0C: b'\x1f\x40',
                     0x0D: b'\x32', 0x0F: b'\x41', 0x11: b'\x40',
                     0x42: b'\x33\x90', 0x5C: b'\x88'}

    def clear(self):
        self.queue.clear()

    def send(self, request):
        self.requests.append(request)
        sid = request[0]
        if sid == 0x22 and len(request) == 3:
            data = self.data.get(int.from_bytes(request[1:], 'big'))
            reply = b'\x62' + request[1:] + data if data is not None else b'\x7f\x22\x31'
        elif sid == 0x01 and len(request) == 2:
            pid = request[1]
            if pid in range(0, 0xE1, 0x20):
                supported = set(self.pids)
                # Continuation flags only for pages containing later supported PIDs.
                for boundary in range(0x20, 0xE1, 0x20):
                    if any(p > boundary for p in self.pids):
                        supported.add(boundary)
                bitmap = sum(1 << (32 - (p - pid)) for p in supported if pid < p <= pid + 32)
                reply = b'\x41' + bytes([pid]) + bitmap.to_bytes(4, 'big')
            else:
                reply = b'\x41' + bytes([pid]) + self.pids[pid] if pid in self.pids else bytes([0x7F, sid, 0x12])
        else:
            reply = bytes([0x7F, sid, 0x11])
        if self.mode == 'silent':
            return
        if self.mode == 'negative':
            reply = bytes([0x7F, sid, 0x22])
        if self.mode == 'wrong_echo':
            reply = reply[:1] + bytes([reply[1] ^ 1]) + reply[2:]
        if self.mode == 'truncated':
            reply = reply[:1]
        if self.mode == 'endless_pending':
            for n in range(100):
                self.queue.append((self.now + .1 * (n + 1), bytes([0x7F, sid, 0x78])))
            return
        if self.mode == 'pending':
            self.queue.append((self.now + .01, bytes([0x7F, sid, 0x78])))
            self.queue.append((self.now + .2, reply))
        else:
            self.queue.append((self.now + .01, reply))

    def recv(self, timeout):
        deadline = self.now + timeout
        if not self.queue or self.queue[0][0] > deadline:
            self.now = deadline
            return None
        arrival, data = self.queue.popleft()
        self.now = arrival
        return data


class DiagnosticLab:
    def __init__(self, ecu, p2=.5, p2_star=1.0, total=3.0):
        if type(ecu) is not FakeECU:
            raise ValueError('Only in-memory FakeECU is supported')
        for duration in (p2, p2_star, total):
            if isinstance(duration, bool) or not isinstance(duration, (int, float)) or not math.isfinite(duration) or duration <= 0:
                raise ValueError('Timeouts must be positive finite numbers')
        self.ecu, self.p2, self.p2_star, self.total = ecu, p2, p2_star, total
        self.trace = []

    def _request(self, request):
        if not isinstance(request, bytes) or not ((len(request) == 3 and request[0] == 0x22) or
                                                  (len(request) == 2 and request[0] == 0x01)):
            raise ValueError('Only ReadDataByIdentifier and Mode 01 are allowed')
        self.ecu.clear()
        deadline = self.ecu.now + self.total
        self.ecu.send(request)
        self.trace.append({'direction': 'tx', 'time': self.ecu.now, 'pdu': request.hex()})
        wait = self.p2
        while True:
            remaining = deadline - self.ecu.now
            if remaining <= 0:
                raise DiagnosticTimeout('Total request deadline exceeded')
            response = self.ecu.recv(min(wait, remaining))
            if response is None:
                raise DiagnosticTimeout('ECU response timed out')
            self.trace.append({'direction': 'rx', 'time': self.ecu.now, 'pdu': response.hex()})
            if not response:
                raise ResponseError('Empty response')
            if response[0] == 0x7F:
                if len(response) != 3 or response[1] != request[0]:
                    raise ResponseError('Malformed or unrelated negative response')
                if response[2] == 0x78:
                    wait = self.p2_star
                    continue  # wait, never resend
                raise NegativeResponse(response[1], response[2])
            if response[0] != request[0] + 0x40:
                raise ResponseError('Unexpected response service')
            return response

    def read_did(self, did, length):
        if type(did) is not int or not 0 <= did <= 0xFFFF or type(length) is not int or not 1 <= length <= 4092:
            raise ValueError('DID and expected length required')
        request = b'\x22' + did.to_bytes(2, 'big')
        response = self._request(request)
        if len(response) != length + 3 or response[1:3] != request[1:]:
            raise ResponseError('DID echo or length mismatch')
        return response[3:]

    def discover_pids(self):
        supported = set()
        for base in range(0, 0xE1, 0x20):
            page = decode_support(base, self._request(bytes([1, base])))
            supported.update(p for p in page if p % 0x20 != 0)
            if base + 32 not in page:
                break
        return supported

    def read_pid(self, pid, supported):
        if type(pid) is not int or pid not in PID_FORMATS:
            raise ValueError('PID decoder not implemented')
        if pid not in supported:
            raise ValueError('PID not advertised by ECU')
        return decode_pid(pid, self._request(bytes([1, pid])))


def run_lab(mode='normal'):
    lab = DiagnosticLab(FakeECU(mode))
    result = {'source': 'fake_ecu', 'mode': mode, 'vehicle_connected': False,
              'readings': [], 'vin': None, 'error': None}
    try:
        result['vin'] = lab.read_did(0xF190, 17).decode('ascii')
        supported = lab.discover_pids()
        result['supported_pids'] = [f'{p:02X}' for p in sorted(supported)]
        result['readings'] = [lab.read_pid(p, supported).__dict__ for p in sorted(supported) if p in PID_FORMATS]
    except DiagnosticError as exc:
        result['error'] = {'type': type(exc).__name__, 'message': str(exc)}
    result['trace'] = lab.trace
    return result


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', choices=FakeECU.MODES, default='normal')
    args = parser.parse_args()
    print(json.dumps(run_lab(args.scenario), ensure_ascii=False, indent=2))
