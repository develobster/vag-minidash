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
