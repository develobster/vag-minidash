"""Bounded candump/schema-1 replay. No bus, session or diagnostic TX is opened."""
import ctypes
import json
import math
from pathlib import Path
import re

from .vag_can_db import VagCanDatabase

LINE = re.compile(r"^\((\d+(?:\.\d+)?)\)\s+([\w.-]+)\s+([0-9a-fA-F]{3}|[0-9a-fA-F]{8})#([0-9a-fA-F]{0,16}|R[0-8]?)$")


def read_log(path, max_frames=100000):
    path = Path(path)
    if type(max_frames) is not int or not 1 <= max_frames <= 1000000 or path.stat().st_size > 64*1024*1024:
        raise ValueError("Replay bounds exceeded")
    with path.open(encoding="utf-8") as source:
        first = source.readline(1025)
    if first.lstrip().startswith("{"):
        from can_capture import CanTraceReader
        yield from CanTraceReader(path, max_frames=max_frames)
        return
    previous, count = -1., 0
    with path.open(encoding="utf-8") as source:
        for number, line in enumerate(iter(lambda: source.readline(1025), ""), 1):
            if len(line) > 1024:
                raise ValueError(f"CAN line {number} exceeds 1024 characters")
            if not line.strip():
                continue
            match = LINE.fullmatch(line.strip())
            if not match:
                raise ValueError(f"Invalid candump line {number}; use timestamped compact candump -L")
            stamp = float(match[1]); raw_id = int(match[3], 16); payload = match[4]
            if not math.isfinite(stamp) or stamp < previous or count >= max_frames:
                raise ValueError("Non-monotonic timestamp or replay limit exceeded")
            previous = stamp;count += 1
            remote = payload.startswith("R")
            if not remote and len(payload) % 2:
                raise ValueError(f"Odd payload length at line {number}")
            yield dict(monotonic=stamp, id=raw_id & 0x1fffffff, extended=len(match[3]) == 8,
                       remote=remote, error=bool(raw_id & 0x20000000), data="" if remote else payload)


class NativeReplay:
    def __init__(self, library, platform, layout, estimate=False):
        config = VagCanDatabase(platform, layout)
        platform, layout = config.platform, config.layout
        self.lib = ctypes.CDLL(str(Path(library).resolve()))
        self.lib.vag_native_create.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_int]
        self.lib.vag_native_create.restype = ctypes.c_void_p
        self.lib.vag_native_destroy.argtypes = [ctypes.c_void_p]
        self.lib.vag_native_feed.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8),
                                           ctypes.c_uint, ctypes.c_uint32, ctypes.c_uint]
        self.lib.vag_native_feed.restype = ctypes.c_uint
        self.lib.vag_native_signal.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint32, ctypes.POINTER(ctypes.c_double)]
        self.lib.vag_native_signal.restype = ctypes.c_uint
        self.lib.vag_native_drag.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32)]
        self.handle = self.lib.vag_native_create(1 if platform == "MQB" else 0, 0 if layout == "sourced" else 1, int(estimate))
        if not self.handle:
            raise ValueError("Native replay allocation/configuration failed")

    def feed(self, row, stamp_ms):
        data = bytes.fromhex(row["data"])
        if len(data) > 8:
            raise ValueError("Classic CAN payload exceeds 8 bytes")
        buffer = (ctypes.c_uint8*8)(*data)
        flags = int(row["extended"]) | (int(row["remote"]) << 1) | (int(row["error"]) << 2)
        self.lib.vag_native_feed(self.handle, row["id"], buffer, len(data), stamp_ms, flags)

    def drag(self):
        values, meta = (ctypes.c_double*6)(), (ctypes.c_uint32*7)()
        self.lib.vag_native_drag(self.handle, values, meta)
        return dict(state=("IDLE", "READY", "RUNNING", "FINISHED")[meta[0]],
                    elapsed_s=values[0], seconds=[values[i+1] if meta[2] & (1 << i) else None for i in range(3)],
                    sample_interval_ms=meta[4], completed_runs=meta[5], reason=meta[1], offline_estimate=True)

    def close(self):
        if self.handle:
            self.lib.vag_native_destroy(self.handle)
            self.handle = None


def replay(path, platform="MQB", layout="sourced", *, max_frames=100000, native_lib=None, estimate_drag=False):
    if estimate_drag and not native_lib:
        raise ValueError("Offline drag estimation requires --native-lib built from this firmware core")
    db = VagCanDatabase(platform, layout)
    native = NativeReplay(native_lib, db.platform, db.layout, estimate_drag) if native_lib else None
    first = None
    try:
        for index, row in enumerate(read_log(path, max_frames), 1):
            if first is None:
                first = row["monotonic"]
            stamp = int(round((row["monotonic"]-first)*1000))
            if not 0 <= stamp <= 86400000:
                raise ValueError("Replay duration exceeds 24 hours")
            db.feed(row["id"], bytes.fromhex(row["data"]), stamp, extended=row["extended"], remote=row["remote"], error=row["error"])
            result = dict(kind="vag_replay", frame=index, stamp_ms=stamp, offline=True, **db.snapshot(stamp))
            if native:
                native.feed(row, stamp)
                if estimate_drag:
                    result["drag"] = native.drag()
            yield result
    finally:
        if native:
            native.close()


def run(args):
    import sys
    output = Path(args.output).open("x", encoding="utf-8") if args.output else sys.stdout
    try:
        for row in replay(args.path, args.platform, args.layout, max_frames=args.max_frames,
                          native_lib=args.native_lib, estimate_drag=args.estimate_drag):
            output.write(json.dumps(row, allow_nan=False) + "\n")
    finally:
        if output is not sys.stdout:
            output.close()
    return 0
