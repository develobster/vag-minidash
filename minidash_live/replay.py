"""Deterministic synthetic UDS transport and recorded passive CAN replay; no vehicle I/O."""
import json
import math
import time
from pathlib import Path


class ReplayClock:
    """Injected monotonic clock; never waits or changes the system clock."""
    def __init__(self):
        self.seconds = 0.0

    def __call__(self):
        return self.seconds

    def advance_to(self, seconds):
        if isinstance(seconds, bool) or not isinstance(seconds, (int, float)) or \
                not math.isfinite(seconds) or seconds < self.seconds:
            raise ValueError("Replay time must be finite and non-decreasing")
        self.seconds = float(seconds)


def replay_can(path, engine, clock, *, max_frames=100000, final_age=0.0,
               on_snapshot=lambda snapshot: None):
    """Feed recorded frames into the existing decoder/store/arbitrator only.

    Deliberately never calls engine.step(), session/discovery or any bus API.
    Caller must inject this fresh clock into engine/store/availability. Outputs
    are offline observations, not new validation evidence. No display bridge.
    """
    from can_capture import CanTraceReader
    if isinstance(final_age, bool) or not isinstance(final_age, (int, float)) or \
            not math.isfinite(final_age) or final_age < 0:
        raise ValueError("Final age must be finite and non-negative")
    if clock() != 0:
        raise ValueError("CAN replay requires a fresh zero clock")
    if engine.clock is not clock or engine.store.clock is not clock or \
            engine.availability.clock is not clock:
        raise ValueError("Replay engine/store/availability must share the replay clock")
    reader = CanTraceReader(path, max_frames=max_frames)
    first = None
    started = time.perf_counter()
    for row in reader:
        if first is None:
            first = row['monotonic']
        clock.advance_to(row['monotonic'] - first)
        engine.feed_can(row['id'], bytes.fromhex(row['data']),
                        extended=row['extended'], remote=row['remote'], error=row['error'])
        on_snapshot({"kind": "replay_sample", "offline": True,
                     "vehicle_validation": False, "replay_seconds": clock(),
                     "frame_index": reader.frames, **engine.snapshot()})
    clock.advance_to(clock() + final_age)
    snapshot = engine.snapshot()
    elapsed = max(0.0, time.perf_counter() - started)
    return {"kind": "replay_summary", "offline": True, "vehicle_validation": False,
            "frames": reader.frames, "trace_complete": reader.complete,
            "timestamp_kind": reader.metadata['timestamp_kind'],
            "replay_seconds": clock(), "processing_seconds": elapsed,
            "frames_per_processing_second": reader.frames / elapsed if elapsed else None,
            **snapshot}


class ReplayTransport:
    def __init__(self, fixture):
        data = json.loads(Path(fixture).read_text(encoding="utf-8"))
        if data.get("kind") != "synthetic_uds_replay_v1":
            raise ValueError("Replay fixture must explicitly be synthetic_uds_replay_v1")
        self.responses = {str(k).lower(): bytes.fromhex(v) for k, v in data.get("responses", {}).items()}
        self.queue = []
        self.sent = []

    def send(self, pdu, timeout):
        self.sent.append(bytes(pdu))
        response = self.responses.get(bytes(pdu).hex())
        if response is not None:
            self.queue.append(response)

    def recv(self, timeout):
        return self.queue.pop(0) if self.queue else None

    def close(self):
        self.queue.clear()


class RecordedUdsTransport:
    """Sequential private captured PDUs, reusable across discovery/session opens.

    No DLL/bus initialization; requests must match order and ECU exactly. Clock
    follows captured receive times; offline execution never promotes evidence.
    The same instance must be shared by successive transport factories.
    """
    def __init__(self, path, *, ecu='engine', clock=None, max_records=100000):
        from .validation import strict_json, read_request
        if ecu != 'engine':
            raise ValueError('First recorded session supports engine only')
        path = Path(path)
        if path.stat().st_size > 32 * 1024 * 1024:
            raise ValueError('Recorded UDS file exceeds 32 MiB')
        self.rows, self.index, self.ecu, self.clock = [], 0, ecu, clock
        previous, active = -1.0, False
        metadata, ended = None, False
        with path.open(encoding='utf-8') as source:
            for number, line in enumerate(source):
                if number >= max_records or len(line) > 1024 * 1024:
                    raise ValueError('Recorded UDS record limit exceeded')
                row = strict_json(line)
                if not isinstance(row, dict) or ended:
                    raise ValueError('Invalid/trailing recorded UDS record')
                stamp = row.get('monotonic')
                if isinstance(stamp, bool) or not isinstance(stamp, (int, float)) \
                        or not math.isfinite(stamp) or stamp < previous:
                    raise ValueError('Invalid/non-monotonic recorded timestamp')
                previous = stamp
                if number == 0:
                    if row.get('kind') != 'metadata' or type(row.get('schema')) is not int \
                            or row['schema'] != 1 or row.get('format') != 'recorded_uds_v1':
                        raise ValueError('Missing recorded_uds_v1 metadata')
                    metadata = row
                    self.origin = stamp
                    continue
                kind = row.get('kind')
                if kind == 'pdu':
                    if row.get('ecu') != ecu or row.get('payload_redacted'):
                        raise ValueError('Unknown ECU or redacted non-replayable payload')
                    value = row.get('pdu')
                    if value is not None and (not isinstance(value, str) or len(value) > 8190
                            or len(value) % 2 or any(c not in '0123456789abcdefABCDEF' for c in value)):
                        raise ValueError('Invalid PDU hex')
                    pdu = bytes.fromhex(value) if value is not None else None
                    if row.get('direction') == 'tx':
                        if active or pdu is None:
                            raise ValueError('Unfinished exchange or empty request')
                        read_request(pdu)
                        active = True
                    elif row.get('direction') == 'rx':
                        if not active:
                            raise ValueError('Response without request')
                        active = pdu == b'\x7f\x22\x78'
                    else:
                        raise ValueError('Unknown PDU direction')
                    self.rows.append(row)
                elif kind == 'fault':
                    if row.get('ecu') != ecu or not isinstance(row.get('error'), str):
                        raise ValueError('Invalid recorded transport fault')
                    self.rows.append(row)
                    active = False
                elif kind == 'end':
                    ended = True
                elif kind not in ('discovery', 'plan', 'source', 'selection', 'acquisition', 'evidence', 'summary'):
                    raise ValueError('Unknown recorded UDS record kind')
        if metadata is None or not ended or active:
            raise ValueError('Incomplete recorded UDS trace; preserve it for manual fault analysis')
        self.metadata = metadata

    def _next(self):
        if self.index >= len(self.rows):
            raise EOFError('Recorded UDS trace exhausted')
        return self.rows[self.index]

    def _advance(self, row):
        if self.clock:
            self.clock.advance_to(row['monotonic'] - self.origin)
        self.index += 1

    def send(self, pdu, timeout):
        row = self._next()
        if row.get('direction') != 'tx' or row['pdu'].lower() != bytes(pdu).hex():
            raise ValueError('Recorded request order mismatch')
        self._advance(row)
        if self.index < len(self.rows) and self.rows[self.index]['kind'] == 'fault':
            self._fault(self.rows[self.index])

    def _fault(self, row):
        self._advance(row)
        raise OSError('Recorded transport fault: ' + row['error'])

    def recv(self, timeout):
        row = self._next()
        if row['kind'] == 'fault':
            self._fault(row)
        if row.get('direction') != 'rx':
            raise ValueError('Recorded response order mismatch')
        self._advance(row)
        return None if row['pdu'] is None else bytes.fromhex(row['pdu'])

    @property
    def exhausted(self):
        return self.index == len(self.rows)

    def close(self):
        # Logical connection ends; chronology remains across reconnects.
        pass
