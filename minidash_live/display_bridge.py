"""Read-only bridge from arbitrated host signals to the embedded UI."""
from dataclasses import dataclass
import math
import queue
import threading
from .instruments import encode_instrument_profile


PROTOCOL_PREFIX = "MDS1"
MAX_TTL_MS = 60_000
MAX_FLOAT32 = 3.4028235e38

UI_SIGNAL_NAMES = frozenset({
    "engine.rpm",
    "vehicle.speed",
    "engine.coolant_temperature",
    "engine.oil_temperature",
    "engine.load",
    "engine.charge_pressure_actual",
    "engine.ambient_pressure",
    "engine.boost",
    "transmission.oil_temperature",
    "transmission.gear",
    "awd.clutch_temperature",
    "vehicle.battery_voltage",
})
UI_STATES = frozenset({"VALID", "STALE", "UNAVAILABLE", "UNSUPPORTED", "ERROR"})


@dataclass(frozen=True)
class DisplaySample:
    canonical_signal: str
    state: str
    ttl_ms: int
    value: float | None = None


def _ttl_ms(rows, preferred=None):
    row = preferred or next(iter(rows), {})
    ttl = row.get("ttl", 1.0)
    if isinstance(ttl, bool) or not isinstance(ttl, (int, float)) or \
            not math.isfinite(ttl) or ttl <= 0:
        return 1000
    return min(MAX_TTL_MS, max(1, math.ceil(ttl * 1000)))


def project_snapshot(snapshot, signal_names):
    """Project selected, trusted runtime rows into the fixed firmware signal set."""
    if not isinstance(snapshot, dict):
        raise ValueError("Runtime snapshot must be an object")
    decisions = snapshot.get("decisions", {})
    rows = snapshot.get("signals", [])
    if not isinstance(decisions, dict) or not isinstance(rows, list):
        raise ValueError("Runtime snapshot is missing decisions or signal rows")

    samples = []
    for name in dict.fromkeys(signal_names):
        if name not in UI_SIGNAL_NAMES:
            raise ValueError(f"Signal is not supported by the embedded UI: {name}")
        decision = decisions.get(name, {})
        if not isinstance(decision, dict):
            decision = {}
        relevant = [row for row in rows if isinstance(row, dict) and row.get("signal") == name]
        selected = decision.get("selected")

        if decision.get("ambiguous"):
            state, value = "ERROR", None
        elif isinstance(selected, dict) and selected.get("quality") == "valid":
            candidate = selected.get("value")
            if isinstance(candidate, bool) or not isinstance(candidate, (int, float)) or \
                    not math.isfinite(candidate) or abs(candidate) > MAX_FLOAT32:
                state, value = "ERROR", None
            else:
                state, value = "VALID", float(candidate)
        else:
            qualities = {row.get("quality") for row in relevant}
            if "error" in qualities or "invalid" in qualities:
                state = "ERROR"
            elif "stale" in qualities:
                state = "STALE"
            elif qualities and qualities <= {"unsupported"}:
                state = "UNSUPPORTED"
            else:
                state = "UNAVAILABLE"
            value = None

        samples.append(DisplaySample(name, state,
                                     _ttl_ms(relevant, selected), value))
    return tuple(samples)


def encode_sample(sample):
    """Encode one bounded, versioned ASCII sample frame."""
    if not isinstance(sample, DisplaySample) or sample.canonical_signal not in UI_SIGNAL_NAMES:
        raise ValueError("Unsupported display signal")
    if sample.state not in UI_STATES:
        raise ValueError("Unsupported display signal state")
    if isinstance(sample.ttl_ms, bool) or not isinstance(sample.ttl_ms, int) or \
            not 1 <= sample.ttl_ms <= MAX_TTL_MS:
        raise ValueError("TTL must be between 1 and 60000 milliseconds")
    if sample.state == "VALID":
        if isinstance(sample.value, bool) or not isinstance(sample.value, (int, float)) or \
                not math.isfinite(sample.value) or abs(sample.value) > MAX_FLOAT32:
            raise ValueError("VALID samples require a finite float32 value")
        value = format(float(sample.value), ".7g")
    else:
        if sample.value is not None:
            raise ValueError("Non-valid samples must not carry a value")
        value = "-"
    return f"{PROTOCOL_PREFIX}|{sample.canonical_signal}|{sample.state}|{sample.ttl_ms}|{value}\n".encode("ascii")


def encode_snapshot(snapshot, signal_names):
    return tuple(encode_sample(sample) for sample in project_snapshot(snapshot, signal_names))


class SerialDisplayBridge:
    """Bounded latest-snapshot writer; serial I/O runs outside the acquisition loop."""
    def __init__(self, serial_port):
        self._serial = serial_port
        self._queue = queue.Queue(maxsize=1)
        self._lock = threading.Lock()
        self._error = None
        self._closed = False
        self.dropped_batches = 0
        self._worker = threading.Thread(target=self._write_loop,
                                        name="minidash-display-serial", daemon=True)
        self._worker.start()

    @property
    def error(self):
        with self._lock:
            return self._error

    def publish(self, snapshot, signal_names, *, instrument_profile=None, profile_ttl_ms=3000):
        frames = encode_snapshot(snapshot, signal_names)
        if instrument_profile is not None:
            # Repeat with every retained batch: a full queue must not drop identity.
            frames = (encode_instrument_profile(instrument_profile, profile_ttl_ms),) + frames
        if not frames:
            return True
        with self._lock:
            if self._closed or self._error is not None:
                return False
            try:
                self._queue.put_nowait(frames)
            except queue.Full:
                try:
                    self._queue.get_nowait()
                except queue.Empty:
                    pass
                else:
                    self._queue.task_done()
                    self.dropped_batches += 1
                try:
                    self._queue.put_nowait(frames)
                except queue.Full:
                    self.dropped_batches += 1
                    return False
        return True

    def _write_loop(self):
        while True:
            with self._lock:
                if self._closed and self._queue.empty():
                    return
            try:
                frames = self._queue.get(timeout=0.1)
            except queue.Empty:
                continue
            try:
                for frame in frames:
                    written = self._serial.write(frame)
                    if written != len(frame):
                        raise IOError(f"partial serial write: {written} of {len(frame)} bytes")
            except Exception as exc:  # Keep a panel disconnect from stopping acquisition.
                with self._lock:
                    self._error = exc
                return
            finally:
                self._queue.task_done()

    def close(self):
        with self._lock:
            if self._closed:
                return
            self._closed = True
        self._worker.join(timeout=2.0)
        try:
            self._serial.close()
        except Exception:
            pass


def open_serial_display_bridge(port, baudrate=115200):
    """Open optional pySerial USB Serial/JTAG transport without toggling DTR/RTS."""
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError("USB display publishing needs pyserial; install requirements-display.txt") from exc
    serial_port = serial.Serial()
    serial_port.port = port
    serial_port.baudrate = baudrate
    serial_port.timeout = 0.1
    serial_port.write_timeout = 0.1
    serial_port.dtr = False
    serial_port.rts = False
    serial_port.open()
    return SerialDisplayBridge(serial_port)
